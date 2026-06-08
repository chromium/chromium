// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This file contains the necessary implementations to serialize and
//! deserialize pending associated endpoints for sending via Mojo messages.
//!
//! Typically, this process is handled internally by the `mojom_value_parser`
//! crate. However, unlike every other type, the (de)serialization process for
//! associated endpoints is stateful: it needs to register them with a
//! `MultiplexRouter`, which is provided as a secondary argument to the process.
//! Since the `MultiplexRouter` type isn't known to the parser crate, we have to
//! handle things here.
//!
//! Rather than passing a router reference directly, we define a `Registrar`
//! trait which provides the ability to register with a router, and then
//! implement the parsing code assuming we have a registrar provided to us.

chromium::import! {
  "//mojo/public/rust/system";
  "//mojo/public/rust/mojom_value_parser";
  "//mojo/public/rust/mojom_value_parser:mojom_value_parser_core";
}

use mojom_value_parser_core::{MojomType, MojomValue};

use crate::interface::DynMojomInterface;
use crate::marker_types::IsRemote;
use crate::multiplex_router::{EndpointInfo, InterfaceId, MultiplexRouterHandle};
use crate::pending_associated_endpoint::{
    AssociatedEndpointState, AssociatedState, PendingAssociatedEndpoint,
};

// We need to implement the `MojomParse` trait manually. This trait is defined
// by the `mojom_value_parser` crate, and specifies how to transform a value
// into/from a `MojomValue`, which is the type used for serialization.
//
// Unlike every other type, pending associated endpoints make use of the
// `MojomParse` trait's `Context` argument. They require that context to be,
// essentially, a reference to a `MultiplexRouter`, which they use to register
// themselves or their entangled endpoint during the
// serialization/deserialization process.

/// This trait abstracts over the `register_new_endpoint` from a
/// `MultiplexRouterHandle`, since sometimes during parsing/desparsing we don't
/// have an actual handle but we still have enough information to register an
/// endpoint.
pub trait Registrar: Send + Sync {
    // The trait needs to be public so we can name it in generated bindings code
    // (as part of `MojomParse<dyn Registrar>`), but we don't actually need to
    // expose any more information about it than the name.
    #[allow(private_interfaces)]
    fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<MultiplexRouterHandle>;
}

impl Registrar for MultiplexRouterHandle {
    fn register_new_endpoint(
        &self,
        interface_id: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<MultiplexRouterHandle> {
        self.register_new_endpoint(interface_id, endpoint_info)
    }
}

// This is a dummy impl that lets us use () as a context during testing.
impl Registrar for () {
    #[allow(private_interfaces)]
    fn register_new_endpoint(
        &self,
        _interface_id: Option<InterfaceId>,
        _endpoint_info: Option<EndpointInfo>,
    ) -> Option<MultiplexRouterHandle> {
        panic!("This implementation only exists for testing, and should never be called!")
    }
}

impl<Context, T, Marker> mojom_value_parser::MojomParse<Context>
    for PendingAssociatedEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized + 'static,
    Marker: IsRemote + 'static,
    Context: Registrar,
{
    fn mojom_type() -> MojomType {
        if Marker::IS_REMOTE {
            MojomType::PendingAssociatedRemote
        } else {
            MojomType::PendingAssociatedReceiver
        }
    }

    fn into_mojom_value(self, context: &Context) -> MojomValue {
        // Unlike other types, we need to do some work using the context.
        // Specifically, we need to register our peer endpoint with the router,
        // and get back the interface ID that was assigned to it so we can
        // register ourselves on the other side of the pipe.
        let AssociatedEndpointState::Shared(shared_state) = self.state else {
            panic!("Cannot serialize an associated interface which has already been sent through a message pipe.")
        };

        let interface_id = AssociatedState::register_with_router(shared_state, context);
        let interface_id_nonzero =
            interface_id.try_into().expect("Should never try to serialize a zero interface ID!");

        if Marker::IS_REMOTE {
            MojomValue::PendingAssociatedRemote(interface_id_nonzero)
        } else {
            MojomValue::PendingAssociatedReceiver(interface_id_nonzero)
        }
    }

    fn try_from_mojom_value(value: MojomValue, context: &Context) -> anyhow::Result<Self> {
        // When we read an associated endpoint from a pipe, we need to alert the
        // `MultiplexRouter` managing the pipe that it exists so it knows how
        // to route messages for that interface ID.
        let is_remote = Marker::IS_REMOTE;
        let interface_id = match value {
            MojomValue::PendingAssociatedRemote(interface_id) if is_remote => interface_id,
            MojomValue::PendingAssociatedReceiver(interface_id) if !is_remote => interface_id,
            _ if is_remote => anyhow::bail!("Expected PendingAssociatedRemote, got {:?}", value),
            _ => anyhow::bail!("Expected PendingAssociatedReceiver, got {:?}", value),
        };

        let router_owned =
            context.register_new_endpoint(Some(interface_id.into()), None).ok_or_else(|| {
                anyhow::anyhow!(
                    "Interface ID {interface_id} was already registered with the router!"
                )
            })?;
        Ok(Self::new_singleton(router_owned))
    }
}
