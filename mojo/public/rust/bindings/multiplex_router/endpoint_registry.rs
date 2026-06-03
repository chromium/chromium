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
pub(super) const PRIMARY_INTERFACE_ID: InterfaceId = 0;
/// These IDs are used for control messages (e.g. disconnect notifications).
pub(super) const CONTROL_INTERFACE_ID_1: InterfaceId = u32::MAX;
pub(super) const CONTROL_INTERFACE_ID_2: InterfaceId = u32::MAX - 1;
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
pub(crate) struct EndpointInfo {
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

/// A `MultiplexRouter`'s internal map of attached endpoints.
///
/// The registry tracks handlers and sequence information for each endpoint that
/// has been registered with the router.
///
/// By default, new interfaces are registered by inserting their ID into the map
/// with a `None` entry. When the endpoint is bound, we update that entry to
/// contain the `EndpointInfo` with binding information. When the endpoint is
/// disconnect it, we remove its entry from the map entirely.
///
/// Since there are about 2^32 possible interface IDs, we do not worry about
/// re-using entries. Each entry has four possible states, and will always
/// move through them in order, unless the corresponding endpoint is dropped:
///
/// For each `InterfaceId`, it will move through:
/// 1. Not yet added: This InterfaceId has never been assigned to an interface.
/// 2. Inbound: The entry exists and maps to `None`. The corresponding endpoint
///    cannot handle message or disconnect notifications.
/// 3. Bound: The entry exists and maps to `Some`.
/// 4. Disconnected: The entry no longer exists in the map.
///
/// States 1 and 4 are identical, but it is impossible to receive a
/// (well-formed) message for an interface ID in state 1, because sending a
/// message requires first sending an associated endpoint, and that process
/// will perform the registration. Hence any messages received for an unknown
/// ID are either malformed or destined for a disconnected interface, and can
/// therefore be ignored.
pub(super) struct EndpointRegistry {
    next_interface_id: u32,
    pub(super) endpoint_map: HashMap<InterfaceId, Option<EndpointInfo>>,
}

impl EndpointRegistry {
    pub(super) fn new(
        sets_high_bit: bool,
        endpoint_map: HashMap<InterfaceId, Option<EndpointInfo>>,
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
        if matches!(
            self.next_interface_id,
            CONTROL_INTERFACE_ID_1 | CONTROL_INTERFACE_ID_2 | HIGH_BIT_MASK
        ) {
            panic!("MultiplexRouter ran out of interface IDs to allocate")
        }
        ret
    }
}
