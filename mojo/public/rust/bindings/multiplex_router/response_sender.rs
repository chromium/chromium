// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The `ResponseSender` type needs to be public so we can name it in other parts
// of the code, but we don't actually need to expose any more information about
// it than the name.
#![allow(private_interfaces)]

use crate::message::MojomMessage;

use super::cpp_interop::CppResponseSender;
use super::endpoint_registry::InterfaceId;
use super::multiplex_router::MultiplexRouter;
use super::AssociatedRouterHandle;

/// This type serves a similar purpose to a `MultiplexRouterHandle` but
/// abstracts over the underlying router implementation (Rust or C++). It is
/// passed into handlers so they can send responses and register new associated
/// endpoints. Unlike `MultiplexRouterHandle`, it does not implement `Drop`
/// because it is short-lived and frequently copied.
pub enum ResponseSender {
    Rust(MultiplexRouter, InterfaceId),
    // The C++ responder object encapsulates the interface ID already
    Cpp(CppResponseSender),
}

impl ResponseSender {
    pub(super) fn rust(router: MultiplexRouter, interface_id: InterfaceId) -> Self {
        Self::Rust(router, interface_id)
    }

    pub(super) fn cpp(cpp_sender: CppResponseSender) -> Self {
        Self::Cpp(cpp_sender)
    }

    /// Creates a copy of this `ResponseSender` that can be used as a
    /// `Registrar` but does not guarantee that trying to send a response will
    /// succeed.
    pub fn registrar_only(&self) -> Self {
        match self {
            Self::Rust(multiplex_router, interface_id) => {
                Self::Rust(multiplex_router.clone(), *interface_id)
            }
            Self::Cpp(cpp_sender) => Self::Cpp(cpp_sender.registrar_only()),
        }
    }

    /// Send a response message through the router with the same interface ID
    /// as the incoming message.
    ///
    /// This function returns `false` if the message wasn't sent, because one
    /// end of the pipe was closed.
    pub fn send_message(&self, msg: MojomMessage) {
        match self {
            Self::Rust(multiplex_router, interface_id) => {
                multiplex_router.send_message(msg, *interface_id)
            }
            Self::Cpp(cpp_sender) => cpp_sender.send_message(msg),
        }
    }
}

impl crate::pending_associated_endpoint_parsing::Registrar for ResponseSender {
    fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<super::EndpointInfo>,
    ) -> Option<AssociatedRouterHandle> {
        match self {
            Self::Rust(multiplex_router, _) => {
                let interface_id =
                    multiplex_router.add_associated_interface(interface_id, endpoint_info)?;
                let handle =
                    super::MultiplexRouterHandle::from_parts(interface_id, multiplex_router);
                Some(AssociatedRouterHandle::Rust(handle))
            }
            Self::Cpp(cpp_sender) => {
                let handle = cpp_sender.register_new_endpoint(interface_id, endpoint_info)?;
                Some(AssociatedRouterHandle::Cpp(handle))
            }
        }
    }
}
