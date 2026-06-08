// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The `ResponseSender` type needs to be public so we can name it in other parts
// of the code, but we don't actually need to expose any more information about
// it than the name.
#![allow(private_interfaces)]

use crate::message::MojomMessage;

use super::endpoint_registry::InterfaceId;
use super::router::MultiplexRouter;

/// This type is identical to a `MultiplexRouterHandle`, but passed into
/// handlers so they can send responses and register new associated endpoints.
/// Unlike `MultiplexRouterHandle`, it does not implement `Drop` because it is
/// frequently cloned, and short-lived.
#[derive(Clone)]
pub struct ResponseSender {
    pub(super) router: MultiplexRouter,
    pub(super) interface_id: InterfaceId,
}

impl ResponseSender {
    /// Send a response message through the router with the same interface ID
    /// as the incoming message.
    ///
    /// This function returns `false` if the message wasn't sent, because one
    /// end of the pipe was closed.
    pub fn send_message(&self, msg: MojomMessage) {
        self.router.send_message(msg, self.interface_id)
    }
}

impl crate::pending_associated_endpoint_parsing::Registrar for ResponseSender {
    fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<super::EndpointInfo>,
    ) -> Option<super::MultiplexRouterHandle> {
        let interface_id = self.router.add_associated_interface(interface_id, endpoint_info)?;
        Some(super::MultiplexRouterHandle::from_parts(interface_id, &self.router))
    }
}
