// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `PendingEndpoint` type, which is the underlying type
//! for `PendingRemote` and `PendingReceiver`.
//!
//! Since both of those have essentially the same behavior, we unify them into
//! a single underlying type. However, since we still want to distinguish
//! between the two, we add a generic parameter to the underlying type, which
//! will be instantiated with one of two marker types.
//!
//! The subset of behavior that's unique to remotes and receivers is
//! implemented only for the appropriate type in `remote.rs` or `receiver.rs`.

chromium::import! {
  "//mojo/public/rust/system";
  "//mojo/public/rust/mojom_value_parser";
  "//mojo/public/rust/mojom_value_parser:mojom_value_parser_core";
}

use std::marker::PhantomData;

use mojom_value_parser_core::{MojomType, MojomValue};
use system::message_pipe::MessageEndpoint;

use crate::interface::DynMojomInterface;
use crate::marker_types::{IsRemote, Receiver, Remote};

/// A `Remote` or `Receiver` that hasn't yet been bound to a sequence/state
/// object, and therefore isn't ready to send or receive messages.
///
/// This type is generic and works for both Remotes and Receivers. The two are
/// distinguished at the type level by the `Marker` parameter, which should be
/// either `Remote` or `Receiver`.
pub struct PendingEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    pub(crate) endpoint: MessageEndpoint,
    // Both `T` and `Marker` are only useful for strong typing.
    // fn() -> T acts like T, but is always `Send` and `Sync`
    #[allow(clippy::type_complexity)]
    _phantom: PhantomData<(fn() -> T, fn() -> Marker)>,
}

pub type PendingRemote<T> = PendingEndpoint<T, Remote>;
pub type PendingReceiver<T> = PendingEndpoint<T, Receiver>;

impl<T, Marker> PendingEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    /// Create a new PendingEndpoint from a raw pipe endpoint.
    ///
    /// If you want to create a new remote/receiver pair, use
    /// `new_pipe` instead. This function is mostly useful for creating a new
    /// `Remote` or `Receiver` from an endpoint received via mojo or FFI.
    ///
    /// Note that the caller is responsible for ensuring that `Self` has the
    /// same instantiation of `T` as the other endpoint, or else incoming
    /// messages will be incomprehensible.
    pub fn new(endpoint: MessageEndpoint) -> Self {
        Self { endpoint, _phantom: PhantomData }
    }

    /// Consume this PendingEndpoint and return the underlying message pipe
    /// endpoint.
    pub fn into_endpoint(self) -> MessageEndpoint {
        self.endpoint
    }

    /// Create a new Mojo message pipe corresponding to `T`'s interface, and
    /// return the endpoints.
    ///
    /// This can only fail if the system has run out of resources to create new
    /// pipes.
    pub fn new_pipe() -> Option<(PendingRemote<T>, PendingReceiver<T>)> {
        let (endpoint1, endpoint2) = MessageEndpoint::create_pipe().ok()?;
        return Some((PendingEndpoint::new(endpoint1), PendingEndpoint::new(endpoint2)));
    }
}

// We need to implement these traits manually because #derive doesn't handle
// PhantomData very well (it imposes requirements on T that aren't needed).

impl<T, Marker> std::fmt::Debug for PendingEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
    Marker: IsRemote,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let ty_str = if Marker::IS_REMOTE { "PendingRemote" } else { "PendingReceiver" };
        f.debug_struct(ty_str).field("endpoint", &self.endpoint).finish()
    }
}

impl<T, Marker> PartialEq for PendingEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    fn eq(&self, other: &Self) -> bool {
        self.endpoint == other.endpoint
    }
}

impl<T, Marker> Eq for PendingEndpoint<T, Marker> where T: DynMojomInterface + ?Sized {}

impl<Context, T, Marker> mojom_value_parser::MojomParse<Context> for PendingEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized + 'static,
    Marker: IsRemote + 'static,
{
    fn mojom_type() -> MojomType {
        if Marker::IS_REMOTE {
            MojomType::PendingRemote
        } else {
            MojomType::PendingReceiver
        }
    }

    fn into_mojom_value(self, _context: &Context) -> MojomValue {
        if Marker::IS_REMOTE {
            MojomValue::PendingRemote(self.into_endpoint())
        } else {
            MojomValue::PendingReceiver(self.into_endpoint())
        }
    }

    fn try_from_mojom_value(value: MojomValue, _context: &Context) -> anyhow::Result<Self> {
        match value {
            MojomValue::PendingRemote(handle) if Marker::IS_REMOTE => Ok(Self::new(handle)),
            MojomValue::PendingReceiver(handle) if !Marker::IS_REMOTE => Ok(Self::new(handle)),
            _ if Marker::IS_REMOTE => anyhow::bail!("Expected PendingRemote, got {:?}", value),
            _ => anyhow::bail!("Expected PendingReceiver, got {:?}", value),
        }
    }
}
