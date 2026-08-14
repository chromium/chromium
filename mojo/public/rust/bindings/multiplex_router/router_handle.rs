// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines handle types which serve as the connection between a
//! `Remote`/`Receiver` and the underlying router. These handles bundle together
//! an ID (for the remote/receiver pair) with a way of sending messages through
//! the router, and registering new pairs with it.
//!
//! Under the hood, there are two kinds of handles we might have:
//! - A `MultiplexRouterHandle` which owns its underlying message pipe endpoint
//! - An _Associated_ `MultiplexRouterHandle` which uses another router's pipe
//!
//! This type abstracts over both of them, so that individual remotes and
//! receivers don't need to care about which type of connection they have.

use std::sync::{Arc, OnceLock};

use crate::message::MojomMessage;
use crate::pending_associated_endpoint_parsing::Registrar;

use super::endpoint_registry::{EndpointInfo, InterfaceId};
use super::multiplex_router_handle::MultiplexRouterHandle;

/// A connection to a router (ultimately a `MultiplexRouter`)
/// which can be used to send/receive messages, and register new endpoints. See
/// the module-level documentation for more details.
pub(crate) enum RouterHandle {
    Primary(MultiplexRouterHandle),
    /// Associated endpoints might be bound (creating a `Remote` or `Receiver`)
    /// before they're actually associated with a specific router. Therefore,
    /// we need an `Arc<OnceLock>` so that the handle can be updated later when
    /// the other endpoint is sent in a message.
    Associated(Arc<OnceLock<AssociatedRouterHandle>>),
}

/// A connection to an existing router, used by associated endpoints.
/// It offers the same functionality as `RouterHandle` but is non-owning.
#[derive(PartialEq, Eq)]
pub(crate) enum AssociatedRouterHandle {
    Rust(MultiplexRouterHandle),
    // We'll add a Cpp variant in the future.
}

impl RouterHandle {
    pub(crate) fn new_associated(handle: AssociatedRouterHandle) -> Self {
        Self::Associated(Arc::new(OnceLock::from(handle)))
    }

    pub(crate) fn send_message(&self, msg: MojomMessage) {
        match self {
            Self::Primary(handle) => handle.send_message(msg),
            Self::Associated(lock) => lock
                .get()
                .expect("Associated Remotes and Receivers cannot be used before the other endpoint has been sent via a message.")
                .send_message(msg),
        }
    }

    // Checks if this router handle has a corresponding pipe and can therefore be
    // used to send and receive messages.
    pub(crate) fn ready_for_messages(&self) -> bool {
        match self {
            Self::Primary(_) => true,
            Self::Associated(lock) => lock.get().is_some(),
        }
    }
}

impl Registrar for RouterHandle {
    fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<AssociatedRouterHandle> {
        match self {
            Self::Primary(handle) => handle
                .register_new_endpoint(interface_id, endpoint_info)
                .map(AssociatedRouterHandle::Rust),
            Self::Associated(lock) => lock
                .get()
                .expect("Associated Remotes and Receivers cannot be used before the other endpoint has been sent via a message.")
                .register_new_endpoint(interface_id, endpoint_info),
        }
    }
}

impl AssociatedRouterHandle {
    pub(crate) fn send_message(&self, msg: MojomMessage) {
        match self {
            Self::Rust(handle) => handle.send_message(msg),
        }
    }

    pub(crate) fn interface_id(&self) -> InterfaceId {
        match self {
            Self::Rust(handle) => handle.interface_id(),
        }
    }

    // This function takes &mut self because binding is a mutable operation.
    // The Rust implementation uses a `Mutex` so `&self` is sufficient, but that
    // will likely change in the future when we make it sequenced.
    pub(crate) fn bind(&mut self, endpoint_info: EndpointInfo) {
        match self {
            Self::Rust(handle) => handle.bind(endpoint_info),
        }
    }
}

impl std::fmt::Debug for AssociatedRouterHandle {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Rust(_) => write!(f, "AssociatedRouterHandle::Rust"),
        }
    }
}

impl Registrar for AssociatedRouterHandle {
    fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<AssociatedRouterHandle> {
        match self {
            Self::Rust(handle) => handle
                .register_new_endpoint(interface_id, endpoint_info)
                .map(AssociatedRouterHandle::Rust),
        }
    }
}
