// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the map which is used by the `MultiplexRouter` type to
//! track its attached endpoints.
//!
//! When a new interface is registered to the router, it generates a new
//! interface ID, which is returned to the caller, and stores the interface's
//! info in the registry; this includes the handler to run when messages arrive,
//! and the sequence to run it on.
//!
//! If the other end of an associated interface has already been assigned an ID,
//! then the registration will use that ID instead.
//!
//! If an endpoint has not yet been bound to a sequence when it is registered,
//! it will have to update the registry upon being bound, or else the handler
//! cannot be run.
//!
//! The `EndpointRegistry` type is analogous to the `endpoints_` member of
//! the C++ `MultiplexRouter` class.

chromium::import! {
  "//mojo/public/rust/system";
  "//base:sequenced_task_runner";
}

use std::collections::HashMap;
use std::sync::Arc;

use sequenced_task_runner::SequencedTaskRunnerHandle;

use crate::message::MojomMessage;

use super::response_sender::ResponseSender;

/// This type represents an ID given out by a multiplex router for an attached
/// endpoint. This type is only visible within the `multiplex_router` submodule,
/// so we assume throughout this file that it's used correctly (i.e. that every
/// `InterfaceId` in the function call was previously registered with the
/// current `MultiplexRouter` object).
pub type InterfaceId = u32;

/// This ID is assigned to the primary interface, which owns the message pipe.
/// Equivalent to `mojo::kPrimaryInterfaceId` in C++.
pub(crate) const PRIMARY_INTERFACE_ID: InterfaceId = 0;
/// This ID is used for control messages (e.g. disconnect notifications).
pub(crate) const CONTROL_INTERFACE_ID: InterfaceId = u32::MAX;
/// It's also used to mean "no interface" in cases where options don't work.
pub(crate) const INVALID_INTERFACE_ID: InterfaceId = CONTROL_INTERFACE_ID;
/// Routers always either generate IDs with the high bit set, or always generate
/// IDs with the high bit unset.
const HIGH_BIT_MASK: InterfaceId = 0x80000000;

// TODO(crbug.com/517522990): Rewrite the handler to call remote/receiver
// functions directly instead of using a trait object, which saves us heap
// allocations and dynamic dispatch.
pub(crate) type MessageHandler = Arc<dyn Fn(MojomMessage, ResponseSender) + Send + Sync + 'static>;
pub(crate) type DisconnectHandler = Box<dyn FnOnce() + Send + 'static>;

/// Contains the relevant information about the remotes and receivers that are
/// attached to this `MultiplexRouter`.
///
/// Specifically, it tracks what function it should run when receiving a message
/// for that interface, as well as the sequence on which to run it, and
/// optionally a disconnect handler to run if the underlying pipe becomes
/// disconnected.
///
/// Note that an endpoint can be registered with the router before being bound
/// to a sequence. If so, it will have to provide this info before it can
/// start processing incoming messages.
pub struct EndpointInfo {
    /// The function to run when a new message arrives at this endpoint.
    pub(crate) incoming_message_handler: MessageHandler,
    /// The task runner on which to schedule the message and disconnect
    /// handlers.
    pub(crate) runner: SequencedTaskRunnerHandle,
    /// The function to run when the other end of the message pipe is closed, if
    /// any. For associated remotes and receiver, this runs when _either_
    /// end of the pipe is closed.
    pub(crate) disconnect_handler: Option<DisconnectHandler>,
}

impl EndpointInfo {
    /// Post the disconnect handler (if any) to this endpoint's runner, and
    /// return `EndpointState::BoundPeerClosed(self)`.
    pub(super) fn handle_peer_closed(mut self) -> EndpointState {
        if let Some(disconnect_handler) = self.disconnect_handler.take() {
            self.runner.post_task(disconnect_handler);
        }
        EndpointState::BoundPeerClosed(self)
    }
}

/// Tracks the state of an `InterfaceId` in the `EndpointRegistry`.
///
/// An `InterfaceId` that is not present in `endpoint_map` has never been seen
/// by this router. Once an entry is inserted, it is never removed from the map.
pub(super) enum EndpointState {
    /// We received a disconnect notification for this ID before we'd even heard
    /// about it. This can happen if we send an associated endpoint whose peer
    /// was already closed.
    UnregisteredPeerClosed,
    /// The endpoint has been registered with the router, but is not currently
    /// bound to a sequence. Incoming messages for this ID will block the
    /// router's queue until `bind_interface` is called.
    Unbound,
    /// As above, but we've also received a disconnect notification for the
    /// peer. If the endpoint is later bound via `bind_interface`, its
    /// disconnect handler will be scheduled immediately.
    UnboundPeerClosed,
    /// The endpoint is bound to a sequence and both sides are connected.
    Bound(EndpointInfo),
    /// The endpoint is alive and bound, but the peer endpoint has closed.
    /// We've already scheduled the disconnect handler, and are just keeping
    /// the endpoint info alive in case C++ has a reference to it.
    BoundPeerClosed(EndpointInfo),
    /// The local endpoint has been dropped/closed.
    Closed,
}

impl EndpointState {
    pub(super) fn endpoint_info(&self) -> Option<&EndpointInfo> {
        match self {
            Self::Bound(info) | Self::BoundPeerClosed(info) => Some(info),
            _ => None,
        }
    }
}

/// A `MultiplexRouter`'s internal map of attached endpoints.
///
/// The registry tracks handlers and sequence information for each endpoint that
/// has been registered with the router. See [`EndpointState`] for the possible
/// states of each entry.
///
/// Since there are about 2^32 possible interface IDs, we do not worry about
/// running out of entries
pub(super) struct EndpointRegistry {
    next_interface_id: u32,
    pub(super) endpoint_map: HashMap<InterfaceId, EndpointState>,
}

impl EndpointRegistry {
    pub(super) fn new(
        sets_high_bit: bool,
        endpoint_map: HashMap<InterfaceId, EndpointState>,
    ) -> Self {
        let next_interface_id = if sets_high_bit { HIGH_BIT_MASK | 1 } else { 1 };
        Self { next_interface_id, endpoint_map }
    }

    /// Return a fresh interface ID which has not yet been used by this
    /// registry.
    ///
    /// Will panic if all IDs have already been used.
    pub(super) fn get_new_interface_id(&mut self) -> InterfaceId {
        let ret = self.next_interface_id;
        self.next_interface_id += 1;
        // Make sure we didn't wrap around or flip the high bit.
        if matches!(self.next_interface_id, CONTROL_INTERFACE_ID | HIGH_BIT_MASK) {
            panic!("MultiplexRouter ran out of interface IDs to allocate")
        }
        ret
    }

    /// Returns whether `id` belongs to the half of the ID space that the peer
    /// allocates from.
    pub(super) fn is_peer_allocated_id(&self, id: InterfaceId) -> bool {
        (id & HIGH_BIT_MASK) != (self.next_interface_id & HIGH_BIT_MASK)
    }
}
