// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `PendingAssociatedEndpoint` type, which is the
//! underlying type for `PendingAssociatedRemote` and
//! `PendingAssociatedReceiver`.
//!
//! Since both of those have essentially the same behavior, we unify them and
//! distinguish between them using a const generic argument. The subset of
//! behavior that's unique to remotes and receivers are implemented only for
//! the appropriate type in `remote.rs` or `receiver.rs`.

// FOR_RELEASE: Remove when associated interfaces are fully implemented
#![allow(unused)]

chromium::import! {
  "//mojo/public/rust/system";
  "//base:sequenced_task_runner";
}

use std::marker::PhantomData;
// TODO(crbug.com/470438844): Figure out which of these should be sequenced
// instead of fully thread-safe
use std::sync::{Arc, Mutex, OnceLock};

use crate::interface::DynMojomInterface;
use crate::marker_types::{IsRemote, Receiver, Remote};
use crate::multiplex_router::{EndpointInfo, InterfaceId, MultiplexRouterHandle};

/// The core state of a pending associated endpoint.
///
/// In practice, each instance of this type is shared with exactly one other
/// endpoint, started when they are created by `new_pair`.
///
/// There are two operations that can change the state: sending an endpoint
/// across an existing pipe, and binding an endpoint to a sequence. The
/// operations can occur in either order. Exactly one endpoint should be
/// serialized, and the other should be bound. Whichever goes first will update
/// the corresponding entry in the shared state (then drop its own copy). When
/// the other operation occurs, it will see the existing entry and use it.
///
/// Upon serialization, the endpoint will create a new `MultiplexRouterHandle`
/// with a fresh interface ID, and store that handle in `router` so the other
/// side can start talking to it once it's bound.
///
/// Upon binding, the endpoint will set `endpoint_info`, so the other endpoint
/// can pass it to the router during serialization.
pub(crate) struct AssociatedState {
    pub(crate) router: Arc<OnceLock<MultiplexRouterHandle>>,
    pub(crate) endpoint_info: Option<EndpointInfo>,
}

pub type SharedAssociatedState = Arc<Mutex<AssociatedState>>;

impl AssociatedState {
    /// Create a new `AssociatedState` that's wrapped in an `Arc` and `Mutex`.
    /// It should be shared between exactly two endpoints.
    pub(crate) fn new_shared() -> SharedAssociatedState {
        Arc::new(Mutex::new(AssociatedState {
            router: Arc::new(OnceLock::new()),
            endpoint_info: None,
        }))
    }

    /// Register a new interface ID with the router, and store a handle for
    /// that ID in the shared state.
    ///
    /// This function should be called when serializing a pending associated
    /// endpoint. It updates the shared state, meaning that the _other_ endpoint
    /// will be the recipient of the newly created handle.
    ///
    /// Returns the interface ID of the newly created handle, to be serialized.
    pub(crate) fn register_with_router(
        shared_state: SharedAssociatedState,
        router_ref: &MultiplexRouterHandle,
    ) -> InterfaceId {
        // This removes the endpoint info from the shared state. This is fine:
        // 1. If it was present, the other side is already finished with the shared
        //    state
        // 2. If it was absent, we lost nothing (and the other side will see that
        //    `router` is present so it won't try to set it later).
        let mut shared_state = shared_state.lock().unwrap();
        let handle = router_ref
            .register_new_endpoint(None, shared_state.endpoint_info.take())
            .expect("Multiplex router should never fail to register if we don't provide an ID");
        let interface_id = handle.interface_id();
        shared_state.router.set(handle).unwrap_or_else(|_| {
            panic!("Exactly one endpoint in each pair should be sent in a message, not both")
        });
        return interface_id;
    }
}

/// Stores the core state of a pending associated endpoint, distinguishing
/// between endpoints which share their state and those that don't.
///
/// New endpoints are always created sharing their state with their partner, but
/// that link is broken when one of the pair is serialized. An endpoint which
/// has been read from the wire will have its own, unshared state. The other
/// endpoint will still be shared, but it will be the only one with a copy of
/// the shared state.
pub(crate) enum AssociatedEndpointState {
    /// Used for endpoints which are created via `new_pair`.
    Shared(SharedAssociatedState),
    /// Used for endpoints which were received via Mojo message.
    Singleton(MultiplexRouterHandle),
}

/// An `AssociatedRemote` or `AssociatedReceiver` that hasn't yet been bound to
/// a sequence/state object, and therefore isn't ready to send or receive
/// messages.
///
/// This type is generic and works for both Remotes and Receivers. The two are
/// distinguished at the type level by the second generic parameter of the type.
pub struct PendingAssociatedEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    pub(crate) state: AssociatedEndpointState,
    // Both `T` and `Marker` are only useful for strong typing.
    // fn() -> T acts like T, but is always `Send` and `Sync`
    #[allow(clippy::type_complexity)]
    _phantom: PhantomData<(fn() -> T, fn() -> Marker)>,
}

pub type PendingAssociatedRemote<T> = PendingAssociatedEndpoint<T, Remote>;
pub type PendingAssociatedReceiver<T> = PendingAssociatedEndpoint<T, Receiver>;

impl<T, Marker> PendingAssociatedEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    pub(crate) fn new_shared(shared_state: SharedAssociatedState) -> Self {
        Self { state: AssociatedEndpointState::Shared(shared_state), _phantom: PhantomData }
    }

    pub(crate) fn new_singleton(handle: MultiplexRouterHandle) -> Self {
        Self { state: AssociatedEndpointState::Singleton(handle), _phantom: PhantomData }
    }

    /// Create a new pair of associated endpoints.
    ///
    /// The pair is not yet associated with any message pipe, so the endpoints
    /// cannot be used to send messages. To enable this, send one of the
    /// endpoints in a Mojo message via an existing remote/receiver pair,
    /// then bind the endpoints (one on each side).
    pub fn new_pair() -> (PendingAssociatedRemote<T>, PendingAssociatedReceiver<T>) {
        let shared_state = AssociatedState::new_shared();
        return (
            PendingAssociatedRemote::new_shared(Arc::clone(&shared_state)),
            PendingAssociatedReceiver::new_shared(shared_state),
        );
    }

    /// Checks if the endpoint has been associated with a specific pipe yet.
    ///
    /// This only happens when the _other_ endpoint is sent via a Mojo message.
    /// If this returns false, trying to use the endpoint (e.g. sending
    /// messages) will panic.
    pub fn can_send_messages(&self) -> bool {
        match &self.state {
            AssociatedEndpointState::Singleton(_) => true,
            AssociatedEndpointState::Shared(shared_state) => {
                shared_state.lock().unwrap().router.get().is_some()
            }
        }
    }

    /// Inform our underlying `MultiplexRouter` that this endpoint has just been
    /// bound to a sequence and is ready to start receiving messages.
    ///
    /// Returns a new handle to that router.
    ///
    /// IMPORTANT: It is not guaranteed that the underlying `MultiplexRouter`
    /// has been set yet. This only happens when the _other_ endpoint is sent
    /// via a Mojo message. In this case, the registration will be delayed until
    /// the router is set, and trying to send a message will panic. This can be
    /// checking using `can_send_messages`.
    pub(crate) fn register_bound(self, endpoint_info: EndpointInfo) -> crate::remote::RouterHandle {
        // If we share our state with the other endpoint, lock the mutex so we can
        // access it. Otherwise, we're a singleton, so we can just register ourselves
        // and return.
        let shared_state = match self.state {
            AssociatedEndpointState::Shared(shared_state) => shared_state,
            AssociatedEndpointState::Singleton(handle) => {
                handle.bind(endpoint_info);
                return Box::new(handle);
            }
        };
        let mut shared_state = shared_state.lock().unwrap();

        // Sanity check, though strictly nothing is wrong with this, it's just
        // completely useless to bind both endpoints.
        assert!(
            shared_state.endpoint_info.is_none(),
            "Exactly one endpoint in each pair should be bound without being send in a message first, not both"
        );

        let router = shared_state.router.clone();
        match router.get() {
            Some(handle) => {
                // If the router handle is already initialized, then we can
                // directly inform it that we're bound now.
                handle.bind(endpoint_info);
            }
            None => {
                // Otherwise, update the binding info in the shared state,
                // so the other endpoint can pass it when it registers us.
                shared_state.endpoint_info = Some(endpoint_info)
            }
        };
        Box::new(router)
    }

    /// Checks if both endpoints are registered with the same interface ID.
    ///
    /// This function is intended for testing, and assumes that its arguments
    /// are a matched pair of endpoints, exactly one of which has been
    /// serialized.
    ///
    /// Will return false if:
    /// - Either endpoint has not been registered
    /// - Both endpoints have been serialized
    /// - Both endpoints are registered, but to different interfaces
    pub fn same_interface_for_testing<OtherMarker>(
        &self,
        other: &PendingAssociatedEndpoint<T, OtherMarker>,
    ) -> bool {
        match (&self.state, &other.state) {
            (
                AssociatedEndpointState::Singleton(handle),
                AssociatedEndpointState::Shared(shared_state),
            )
            | (
                AssociatedEndpointState::Shared(shared_state),
                AssociatedEndpointState::Singleton(handle),
            ) => {
                handle.interface_id()
                    == shared_state.lock().unwrap().router.get().unwrap().interface_id()
            }
            _ => false,
        }
    }
}

//////////////////////////////
// Trait implementations
//////////////////////////////

// This trait impl is required to use Arc<OnceLock<MultiplexRouterHandle>> as
// a router in remotes/receivers.
// TODO(crbug.com/517519181): We may be able to remove this if we make remotes
// and receivers hold an enum instead of a trait object.
impl AsRef<MultiplexRouterHandle> for Arc<OnceLock<MultiplexRouterHandle>> {
    fn as_ref(&self) -> &MultiplexRouterHandle {
        // Use `PendingAssociatedEndpoint::can_send_messages`to see if this is
        // going to panic.
        self.get().expect("Associated Remotes and Receivers cannot be used before the other endpoint has been sent via a message.")
    }
}

// We need to implement some standard traits (Debug, PartialEq) because the
// derive macros don't handle phantom data well (they require that T implements
// the trait even though it doesn't actually need to).

impl<T, Marker> std::fmt::Debug for PendingAssociatedEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
    Marker: IsRemote + 'static,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // There's no good way to print a pending endpoint, so for debugging purposes
        // we print:
        // 1. The address of `shared_state`
        // 2. Whether the handle has been set.
        // 3. Whether the endpoint info has been set.
        //
        // (1) serves as an identifier and lets us connect pairs of associated
        // endpoints, (2) lets us trace the operations that have happened so
        // far.

        let endpoint_type = if Marker::IS_REMOTE { "Pending Remote" } else { "Pending Receiver" };

        match &self.state {
            AssociatedEndpointState::Singleton(handle) => {
                write!(f, "<{endpoint_type}. Interface ID: {}>", handle.interface_id())
            }
            AssociatedEndpointState::Shared(shared_state) => {
                if let Ok(guard) = shared_state.try_lock() {
                    write!(
                        f,
                        "<{endpoint_type}. Addr: {shared_state:p}. Interface ID: {:?}. Has endpoint_info: {}>",
                        guard.router.get().map(MultiplexRouterHandle::interface_id),
                        guard.endpoint_info.is_some()
                    )
                } else {
                    write!(f, "<{endpoint_type}. Addr: {shared_state:p}. (locked)>",)
                }
            }
        }
    }
}

/// Two endpoints are considered equal if they point to the same underlying
/// router and have the same interface ID (if any). Each endpoint is equal to
/// its paired endpoint before either has been serialized. Once one member of
/// the pair has been sent in a message, both sides will be equal only to
/// themselves.
impl<T, Marker> PartialEq for PendingAssociatedEndpoint<T, Marker>
where
    T: DynMojomInterface + ?Sized,
{
    fn eq(&self, other: &Self) -> bool {
        match (&self.state, &other.state) {
            (
                AssociatedEndpointState::Singleton(handle1),
                AssociatedEndpointState::Singleton(handle2),
            ) => handle1 == handle2,
            (
                AssociatedEndpointState::Shared(shared_state1),
                AssociatedEndpointState::Shared(shared_state2),
            ) => Arc::ptr_eq(shared_state1, shared_state2),
            _ => false,
        }
    }
}

impl<T, Marker> Eq for PendingAssociatedEndpoint<T, Marker> where T: DynMojomInterface + ?Sized {}
