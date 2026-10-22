// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the `MultiplexRouter` type proper, which is responsible
//! for tagging outgoing messages with interface IDs, and using them to
//! directing incoming messages to the correct endpoint.
//!
//! This type is only visible within the `multiplex_router` submodule; the rest
//! of the crate will only access it via a `MultiplexRouterHandle` object.
//! Therefore, we make certain assumptions throughout the code, e.g. that all
//! `InterfaceId`s are valid.
//!
//! Implementation Details:
//!
//! A `MultiplexRouter` contains a registry of associated endpoints, which it
//! uses to route incoming messages by scheduling the endpoint's handlers on
//! that endpoint's sequence.
//!
//! It also has a queue of events (`Task`s, either an incoming message or a
//! disconnect notification). Each task is destined for a specific endpoint.
//! However, if an endpoint hasn't been bound to a sequence yet, we can't
//! process any of its tasks. To maintain FIFO ordering, we can't process any
//! later tasks until we're unblocked, so we store the remaining tasks in the
//! queue.
//!
//! The `MultiplexRouter` type is fully thread-safe, though this may change in
//! the future if we need to add a way of unbinding the router (C++ allows this
//! but it's a smell, so we hope to not support it in Rust). The registry and
//! task queue are both stored behind a full `Arc<Mutex<>>` and so can be
//! accessed from any thread. To reduce lock contention, this class never runs
//! handlers directly; it only ever schedules them.

chromium::import! {
  "//mojo/public/rust/system";
  "//base:scoped_refptr";
  "//base:sequenced_task_runner";
}

use std::collections::{HashMap, VecDeque};
use std::sync::{Arc, Mutex, Weak};

use scoped_refptr::ScopedRefPtr;
use sequenced_task_runner::SequencedTaskRunnerHandle;
use system::message_pipe::MessageEndpoint;

use super::arc_or_weak::ArcOrWeak;
use super::control_messages::{
    construct_peer_endpoint_closed_message, parse_incoming_control_message, RunOrClosePipeInput,
};
use super::cpp_interop::ffi;
use super::cpp_interop::CppRouterHandle;

use crate::message::{MojomMessage, ReadableBytesOnlyMessage};
use crate::message_pipe_watcher::{MessagePipeWatcher, ResponseSender};

pub(crate) use super::endpoint_registry::*;

/// A `Task` object represents an incoming event that needs to be processed in
/// FIFO order.
///
/// The two possible types of task are an incoming message, or a disconnection
/// notification. Tasks are stored in a queue inside a `MultiplexRouter`; they
/// may not be handled immediately if they're stuck behind a different task that
/// can't be processed (e.g. because it's destined for an interface that hasn't
/// been bound yet).
enum Task {
    Message(MojomMessage),
    /// If the notification came from the peer, this holds the raw message it
    /// arrived in, so that we can report it if it turns out to be invalid.
    /// Notifications that we generate ourselves don't have one.
    Disconnect(InterfaceId, Option<ReadableBytesOnlyMessage>),
}

/// An object which is responsible for sending and receiving messages on a
/// single message pipe, ensuring that each reaches the sender's paired
/// endpoint.
///
/// This object is never held directly; instead, remotes and receivers hold a
/// `MultiplexRouterHandle`, which is analogous to an Arc, but with extra
/// functionality, including a drop handler and tracking the interface ID.
#[derive(Clone)]
pub struct MultiplexRouter {
    // This will usually be a weak reference, except for the primary endpoint.
    endpoint_watcher: ArcOrWeak<MessagePipeWatcher>,
    shared_state: Arc<Mutex<MultiplexRouterSharedState>>,
}

pub(super) struct MultiplexRouterSharedState {
    /// A map from interface ID to information about the endpoint with that ID.
    registry: EndpointRegistry,
    /// A queue which stores messages and disconnect notifications that we
    /// haven't processed yet.
    ///
    /// This is necessary because it's possible for the router to receive
    /// messages for an endpoint that hasn't yet been bound to a sequence, and
    /// therefore can't yet process messages.
    ///
    /// Since we guarantee total FIFO ordering among messages, if one message is
    /// blocked then _all_ subsequent messages are blocked. We can't put them
    /// back in the pipe, so we store them here until the queue starts moving
    /// again.
    unscheduled_tasks: VecDeque<Task>,
    /// Tracks whether the primary pipe has been disconnected; if so, we'll
    /// schedule the disconnect handler for any new endpoints as soon as they're
    /// bound.
    pipe_closed: bool,
    /// The C++ `AssociatedGroupController` for this router, if any C++
    /// endpoint has needed one. This is just a cache so that new
    /// C++ endpoints re-use the same controller; each endpoint
    /// stores its own reference to the controller. The controller
    /// also has a reference back to the router, so to prevent
    /// reference cycles we enforce the following invariant:
    /// Invariant: this field is only `Some` while the pipe is
    /// connected.
    cpp_group_controller: Option<ScopedRefPtr<ffi::RustAssociatedGroupController>>,
}

impl MultiplexRouter {
    /// Create a new MultiplexRouter that wraps `endpoint`.
    ///
    /// If `sets_high_bit` is true, then interface IDs created by this router
    /// will have their high bit set to 1; otherwise, it will be set to 0.
    ///
    /// Note that this is always called for the primary interface (never an
    /// associated one, which is created from an existing router).
    pub(super) fn new(
        endpoint: MessageEndpoint,
        sets_high_bit: bool,
        endpoint_info: EndpointInfo,
    ) -> Self {
        let runner = endpoint_info.runner.clone();

        let mut endpoint_map = HashMap::new();
        endpoint_map.insert(PRIMARY_INTERFACE_ID, EndpointState::Bound(endpoint_info));

        let shared_state = Arc::new(Mutex::new(MultiplexRouterSharedState {
            registry: EndpointRegistry::new(sets_high_bit, endpoint_map),
            unscheduled_tasks: VecDeque::new(),
            pipe_closed: false,
            cpp_group_controller: None,
        }));
        let shared_state_clone = Arc::clone(&shared_state);

        // We need the watcher to have a reference to the router, so that it can
        // use it for registering associated endpoints when they are sent or
        // received in a message. We need it to hold a weak reference to the
        // watcher in order to avoid a ref cycle. `new_cyclic` lets us create
        // that a weak reference before the watcher is actually initialized.
        let endpoint_watcher = Arc::new_cyclic(move |weak_watcher_ref| {
            {
                let router_weak = Self {
                    endpoint_watcher: ArcOrWeak::Weak(weak_watcher_ref.clone()),
                    shared_state: shared_state_clone,
                };
                let router_weak_clone = router_weak.clone();
                MessagePipeWatcher::new_with_runner(
                    endpoint,
                    runner,
                    move |msg, sender| router_weak.incoming_message_handler(msg, sender),
                    Some(Box::new(move || router_weak_clone.queue_all_disconnect_handlers())),
                    /* begin_processing_immediately = */ false,
                )
            }
            // This can only fail if we're unable to allocate a new mojo handle,
            // which is pretty much unrecoverable.
            .unwrap()
        });

        endpoint_watcher.begin_processing();

        MultiplexRouter { endpoint_watcher: ArcOrWeak::Strong(endpoint_watcher), shared_state }
    }

    /// Create a MultiplexRouter that isn't bound to any pipe, for use in
    /// testing serialization/deserialization logic
    pub(super) fn new_for_testing(sets_high_bit: bool) -> Self {
        let initial_map = HashMap::from([(PRIMARY_INTERFACE_ID, EndpointState::Unbound)]);
        Self {
            endpoint_watcher: ArcOrWeak::Weak(Weak::new()),
            shared_state: Arc::new(Mutex::new(MultiplexRouterSharedState {
                registry: EndpointRegistry::new(sets_high_bit, initial_map),
                unscheduled_tasks: VecDeque::new(),
                pipe_closed: false,
                cpp_group_controller: None,
            })),
        }
    }

    /// Add a new associated endpoint to this router and return its ID.
    ///
    /// If `interface_id` is `Some`, then the contained value will be used;
    /// otherwise, the router will generate a fresh ID. The provided callbacks
    /// will be invoked when a message or disconnect notification arrives
    /// for that ID.
    ///
    /// If `endpoint_info` is `None`, then this endpoint will need to call
    /// `bind_interface` later on before it will start receiving messages.
    ///
    /// Will panic if this router has run out of IDs to allocate.
    ///
    /// Will return `None` if called with an interface ID that is already
    /// registered with the router, or if it is invalid. This should only
    /// happen if we receive a malformed mojo message.
    ///
    /// Note: This function is only called for _associated_ endpoints, never the
    /// primary one (those use `new` instead).
    pub(super) fn add_associated_interface(
        &self,
        interface_id_opt: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<InterfaceId> {
        if matches!(interface_id_opt, Some(PRIMARY_INTERFACE_ID | CONTROL_INTERFACE_ID)) {
            return None;
        }
        // Will be initialized in the block below, while shared_state is locked.
        let interface_id;
        {
            let mut shared_state = self.shared_state.lock().unwrap();
            interface_id = match interface_id_opt {
                // We should only ever get IDs allocated by the peer.
                Some(id) if !shared_state.registry.is_peer_allocated_id(id) => return None,
                Some(id) => id,
                None => shared_state.registry.get_new_interface_id(),
            };

            match shared_state.registry.endpoint_map.entry(interface_id) {
                std::collections::hash_map::Entry::Occupied(mut entry) => {
                    // The peer already closed this endpoint before we
                    // registered it locally
                    if !matches!(entry.get(), EndpointState::UnregisteredPeerClosed) {
                        // We should never try to add an interface ID that was
                        // already registered, since each pair should be unique.
                        // The only way this can happen is if we get a malicious
                        // mojo message.
                        return None;
                    }

                    *entry.get_mut() = match endpoint_info {
                        Some(info) => info.handle_peer_closed(),
                        None => EndpointState::UnboundPeerClosed,
                    };
                }
                std::collections::hash_map::Entry::Vacant(entry) => {
                    let new_state = match endpoint_info {
                        Some(info) => EndpointState::Bound(info),
                        None => EndpointState::Unbound,
                    };
                    entry.insert(new_state);

                    // If the underlying message pipe has been disconnected,
                    // then we should schedule the disconnect task for any new
                    // endpoints. This is possible if someone tries to send an
                    // associated remote/receiver across a pipe that's already
                    // closed.
                    if shared_state.pipe_closed {
                        shared_state
                            .unscheduled_tasks
                            .push_back(Task::Disconnect(interface_id, None));
                    }
                }
            }
        }

        self.schedule_all_possible_tasks();

        return Some(interface_id);
    }

    /// Allocates a new interface ID and associates an unassociated C++ endpoint
    /// with this router using it.
    pub(super) fn associate_interface(&self, handle: CppRouterHandle) -> Option<InterfaceId> {
        let id = self.add_associated_interface(None, None)?;
        handle.associate_with_rust_router(self, id);
        Some(id)
    }

    /// Indicate that the endpoint associated with `interface_id` has been bound
    /// to a sequence, and is ready to process messages on that sequence.
    ///
    /// Optionally the user may provide a disconnect handler, which will be run
    /// if the endpoint can no longer receive messages from the other side of
    /// the pipe.
    pub(crate) fn bind_interface(&self, interface_id: InterfaceId, endpoint_info: EndpointInfo) {
        {
            let mut shared_state = self.shared_state.lock().unwrap();
            let state =
                shared_state.registry.endpoint_map.get_mut(&interface_id).unwrap_or_else(|| {
                    panic!("Endpoint {interface_id} was not registered prior to binding")
                });
            *state = match state {
                EndpointState::Unbound => EndpointState::Bound(endpoint_info),
                EndpointState::UnboundPeerClosed => endpoint_info.handle_peer_closed(),
                EndpointState::Bound(_) | EndpointState::BoundPeerClosed(_) => {
                    panic!("Endpoint {interface_id} was already bound to this router")
                }
                EndpointState::Closed => {
                    panic!("Endpoint {interface_id} was already closed")
                }
                EndpointState::UnregisteredPeerClosed => {
                    panic!("Endpoint {interface_id} was not registered before binding")
                }
            };
        }
        self.schedule_all_possible_tasks();
    }

    /// Send a message through the underlying pipe with the given interface ID.
    pub(super) fn send_message(&self, mut msg: MojomMessage, interface_id: InterfaceId) {
        msg.header.interface_id = interface_id;
        // If the message fails to send, then Mojo will close any attached
        // handles for us. But we have to close any attach associated
        // interfaces ourselves, so get the list of interface IDs (if
        // any) before we give away the message.
        let ids = msg.associated_interface_ids();
        let sent = self.send_raw_message(interface_id, msg.into());
        if !sent {
            for id in ids {
                // We should never have invalid IDs in this array
                debug_assert!(id != PRIMARY_INTERFACE_ID && id != INVALID_INTERFACE_ID);
                // The IDs were registered with this router prior to the sending
                // process, so we need to notify _ourselves_
                // that they've just been dropped.
                self.notify_peer_closed(id);
            }
        }
    }

    /// Tell the router that this end of an interface has been closed, so it
    /// should send a disconnect notification to the other end.
    ///
    /// If the interface ID is 0 (the primary interface), that means that the
    /// entire pipe is closed, so instead we send disconnect messages to all
    /// the _associated_ interfaces on this side. The other endpoint's router
    /// will handle the notification to the interfaces on the other side.
    pub(crate) fn notify_dropped(&self, interface_id: InterfaceId) {
        // Remove the previous entry from the map, and replace it with `Closed`.
        let previous = {
            let mut shared_state = self.shared_state.lock().unwrap();
            let state = shared_state
                .registry
                .endpoint_map
                .get_mut(&interface_id)
                .expect("Attempted to drop an unregistered endpoint");
            std::mem::replace(state, EndpointState::Closed)
        };
        match previous {
            EndpointState::Closed => panic!("Attempted to drop an already-closed endpoint"),
            EndpointState::UnregisteredPeerClosed
            | EndpointState::UnboundPeerClosed
            | EndpointState::BoundPeerClosed(_) => {
                // If the peer already closed, we don't need to notify the peer
                // that our side closed as well, so nothing special to do.
            }
            EndpointState::Unbound | EndpointState::Bound(_) => {
                if interface_id == PRIMARY_INTERFACE_ID {
                    // If the primary interface is being dropped, then we don't
                    // need to notify it of anything, but we do need to alert
                    // all the associated interfaces on this side that they've
                    // been disconnected.
                    // Note that we just set the entry for the primary ID to
                    // `Closed`, so this will run all _other_ disconnect
                    // handlers.
                    self.queue_all_disconnect_handlers();
                } else {
                    // Otherwise, we need to alert the other side that it is now
                    // disconnected. Mojo handles this automatically for the
                    // primary interface, but other interfaces need to send
                    // a special control message.
                    let msg = construct_peer_endpoint_closed_message(interface_id);
                    self.endpoint_watcher.with(|watcher| {
                        // The send can only fail if the entire other side is
                        // closed, in which case we don't need to tell it
                        // anything.
                        let _ = watcher.send_message(msg);
                    });
                }
            }
        }

        self.schedule_all_possible_tasks();
    }

    /// Tell the router that the peer of this associated interface has been
    /// closed, so the local endpoint should be disconnected.
    ///
    /// This is meant for FFI use; normally disconnect handlers are scheduled
    /// via an incoming notification from mojo, but it's also possible for C++
    /// to notify us instead.
    pub(crate) fn notify_peer_closed(&self, interface_id: InterfaceId) {
        let mut shared_state = self.shared_state.lock().unwrap();
        if matches!(
            shared_state.registry.endpoint_map.get(&interface_id),
            Some(EndpointState::Unbound | EndpointState::Bound(_))
        ) {
            shared_state.unscheduled_tasks.push_back(Task::Disconnect(interface_id, None));
        }
        drop(shared_state);
        // Doesn't actually run the disconnect handler yet, just schedules it
        self.schedule_all_possible_tasks();
    }

    /// Sends a raw message through the underlying pipe if the interface is
    /// connected.
    pub(super) fn send_raw_message(
        &self,
        interface_id: InterfaceId,
        msg: system::message::SendableMessage,
    ) -> bool {
        let is_connected = matches!(
            self.shared_state.lock().unwrap().registry.endpoint_map.get(&interface_id),
            Some(EndpointState::Bound(_))
        );
        if is_connected {
            self.endpoint_watcher
                .with(|watcher| watcher.send_message(msg))
                .is_some_and(|result| result.is_ok())
        } else {
            false
        }
    }

    /// Read an incoming message from the wire, and route it to the appropriate
    /// interface.
    ///
    /// This is the overall event handler for the `MultiplexRouter`, which is
    /// run whenever there's an incoming message.
    fn incoming_message_handler(
        &self,
        raw_message: system::message::ReadableWithHandlesMessage,
        _sender: ResponseSender,
    ) {
        let Some(message) = MojomMessage::parse_raw_or_report_bad_message(raw_message) else {
            // If conversion failed then the header must have been malformed.
            // The bad message has already been reported, so nothing else to do.
            return;
        };

        let task = if message.header.interface_id != CONTROL_INTERFACE_ID {
            Task::Message(message)
        } else {
            // If this is a control message, then we need to read it and create
            // the appropriate task, which at the moment will only ever be a
            // disconnect notification.
            if let Some((RunOrClosePipeInput::PeerAssociatedEndpointClosedEvent(event), message)) =
                parse_incoming_control_message(message)
            {
                // We can't tell whether the ID is valid until the task is
                // processed, so keep the message in case we need to report it.
                Task::Disconnect(event.id, Some(message))
            } else {
                // Ignore other types of control message for now, and bad
                // messages which have already been reported.
                return;
            }
        };

        // Add this task to the queue to be scheduled later. If the queue is
        // empty, this will just schedule it immediately.
        self.shared_state.lock().unwrap().unscheduled_tasks.push_back(task);

        self.schedule_all_possible_tasks();
    }

    /// Schedule each queued task on its runner, until we find a task that can't
    /// be scheduled because it's not yet bound to a runner.
    ///
    /// Since we need to guarantee that incoming messages are scheduled in FIFO
    /// order, we can't just process all messages that have a runner; we need to
    /// stop whenever we hit one that we can't handle yet.
    fn schedule_all_possible_tasks(&self) {
        let mut shared_state = self.shared_state.lock().unwrap();
        // Invalid notifications from the peer. Reporting them runs arbitrary
        // embedder code, so we wait until we've released the lock.
        let mut bad_messages = Vec::new();
        // We pop unconditionally because we need to transfer ownership into
        // `try_schedule_message_task`. It's inconvenient but much nicer code.
        while let Some(task) = shared_state.unscheduled_tasks.pop_front() {
            match task {
                Task::Disconnect(interface_id, message) => {
                    if let Err(error) =
                        Self::schedule_disconnect_task(&mut shared_state.registry, interface_id)
                    {
                        let message =
                            message.expect("Generated an invalid disconnect notification");
                        bad_messages.push((message, error));
                    }
                }
                Task::Message(message) => {
                    // `try_schedule_message_task` returns `message` unchanged
                    // on failure
                    if let Err(message) =
                        self.try_schedule_message_task(&shared_state.registry, message)
                    {
                        shared_state.unscheduled_tasks.push_front(Task::Message(message));
                        break;
                    }
                }
            }
        }
        drop(shared_state);

        for (mut message, error) in bad_messages {
            let _ = message.report_bad_message(&error);
        }
    }

    /// Process a disconnect notification for `interface_id`.
    ///
    /// Disconnect tasks never block the queue: if the target endpoint is not
    /// yet bound, its state is updated so that the disconnect handler will
    /// be run when `bind_interface` is called.
    ///
    /// Returns an error if a well-behaved peer could never have sent this
    /// notification. The ones we generate ourselves are always for registered
    /// endpoints, so they never fail.
    fn schedule_disconnect_task(
        registry: &mut EndpointRegistry,
        interface_id: InterfaceId,
    ) -> Result<(), String> {
        let Some(state) = registry.endpoint_map.get_mut(&interface_id) else {
            // This interface ID has not been registered yet. That's fine if the
            // peer allocated it, since we may not have deserialized the message
            // that introduces it yet. Otherwise, the peer is talking about an
            // ID we never allocated.
            if interface_id == PRIMARY_INTERFACE_ID
                || interface_id == CONTROL_INTERFACE_ID
                || !registry.is_peer_allocated_id(interface_id)
            {
                return Err(format!(
                    "Received disconnect notification for invalid interface ID {interface_id}"
                ));
            }
            registry.endpoint_map.insert(interface_id, EndpointState::UnregisteredPeerClosed);
            return Ok(());
        };

        let mut result = Ok(());
        *state = match std::mem::replace(state, EndpointState::Closed) {
            EndpointState::Unbound => EndpointState::UnboundPeerClosed,
            EndpointState::Bound(endpoint_info) => endpoint_info.handle_peer_closed(),
            EndpointState::UnregisteredPeerClosed => {
                // Only the peer sends notifications for unregistered endpoints,
                // and it only sends one per endpoint.
                result = Err(format!(
                    "Received duplicate disconnect notification for interface ID {interface_id}"
                ));
                EndpointState::UnregisteredPeerClosed
            }
            other @ (EndpointState::Closed
            | EndpointState::UnboundPeerClosed
            | EndpointState::BoundPeerClosed(_)) => {
                // These can validly occur if endpoints were dropped while
                // the disconnect task was sitting in the queue.
                other
            }
        };
        result
    }

    /// Schedule an incoming message on its endpoint's runner, or return
    /// `Err(message)` if the target endpoint is currently unbound.
    fn try_schedule_message_task(
        &self,
        registry: &EndpointRegistry,
        message: MojomMessage,
    ) -> Result<(), MojomMessage> {
        let interface_id = message.header.interface_id;
        match registry.endpoint_map.get(&interface_id) {
            Some(EndpointState::Unbound) => {
                // This ID has been registered, but hasn't been bound to a
                // sequence yet. We can't handle this message until that
                // happens. To preserve FIFO ordering, we can't
                // process any later tasks either, so return the
                // message to be put back at the front of
                // the queue.
                Err(message)
            }
            Some(EndpointState::Bound(endpoint_info)) => {
                let handler_clone = endpoint_info.incoming_message_handler.clone();
                let router_clone = self.clone();
                let response_sender = super::ResponseSender::rust(router_clone, interface_id);
                endpoint_info.runner.post_task(move || (*handler_clone)(message, response_sender));
                Ok(())
            }
            None
            | Some(
                EndpointState::Closed
                | EndpointState::UnregisteredPeerClosed
                | EndpointState::UnboundPeerClosed
                | EndpointState::BoundPeerClosed(_),
            ) => {
                // All of these can validly occur through various race
                // conditions. Regardless of how, we can't ever
                // process these messages, so do nothing and let
                // the message drop.
                Ok(())
            }
        }
    }

    /// Queue a task to run the disconnect handler for each registered
    /// interface.
    ///
    /// This is the overall disconnect handler for the router, which is run
    /// when the underlying pipe is closed. Note that it does not transition
    /// entries immediately, because there may still be messages in the queue
    /// that need to be handled before the disconnect handler runs.
    fn queue_all_disconnect_handlers(&self) {
        {
            let mut shared_state_guard = self.shared_state.lock().unwrap();
            let shared_state = &mut *shared_state_guard;
            let registry = &mut shared_state.registry;
            let unscheduled_tasks = &mut shared_state.unscheduled_tasks;

            for (interface_id, state) in registry.endpoint_map.iter() {
                if matches!(state, EndpointState::Unbound | EndpointState::Bound(_)) {
                    unscheduled_tasks.push_back(Task::Disconnect(*interface_id, None));
                }
                // Other states in `endpoint_map` are either already dropped
                // locally, or their peers are closed so we've already
                // run/scheduled their handlers.
            }

            // This will make all future endpoints schedule their disconnect
            // handlers immediately.
            shared_state.pipe_closed = true;
            // The cached controller holds a reference back to this router, so
            // we have to drop it to break the cycle. C++ endpoints
            // that are still alive hold their own references, so
            // they are unaffected.
            drop(shared_state.cpp_group_controller.take());
        }
        self.schedule_all_possible_tasks();
    }

    /// Clone the router (making a new pair of references to its data), but only
    /// keep a weak reference to the underlying endpoint.
    pub(super) fn clone_and_downgrade(&self) -> Self {
        let endpoint_watcher = self.endpoint_watcher.clone_and_downgrade();
        Self { endpoint_watcher, shared_state: self.shared_state.clone() }
    }

    /// Compares two `MultiplexRouters` to see if they point to the same
    /// registry information, ignoring their reference to the watcher.
    pub(super) fn same_registry(&self, other: &Self) -> bool {
        Arc::ptr_eq(&self.shared_state, &other.shared_state)
    }

    /// Return a reference to the C++ `AssociatedGroupController` for this
    /// router, creating it if it doesn't exist yet.
    ///
    /// The controller holds a reference back to this router. To avoid a
    /// reference cycle, a controller is only cached while the pipe is still
    /// usable; any endpoints that are added after that point get a
    /// freshly-created controller.
    pub(super) fn cpp_group_controller(&self) -> ScopedRefPtr<ffi::RustAssociatedGroupController> {
        let mut shared_state = self.shared_state.lock().unwrap();
        if let Some(controller) = shared_state.cpp_group_controller.as_ref() {
            return controller.clone();
        }

        // The controller uses the primary endpoint's sequence,
        // if it exists. Otherwise, whatever sequence it's created on.
        let runner = shared_state
            .registry
            .endpoint_map
            .get(&PRIMARY_INTERFACE_ID)
            .and_then(EndpointState::endpoint_info)
            .map(|info| info.runner.clone())
            .or_else(SequencedTaskRunnerHandle::get_current_default)
            .expect("Mojo endpoints must be used in a sequenced context");

        // The controller only ever gets a weak reference to the pipe, so that
        // C++ endpoints can't keep it alive on their own.
        let controller_ptr = ffi::CreateGroupControllerForRustRouter(
            Box::new(self.clone_and_downgrade()),
            runner.as_scoped_refptr().as_pin(),
        );
        // SAFETY: The controller was just created, and the returned pointer
        // owns one of its ref-counts.
        let controller = unsafe { ScopedRefPtr::wrap_ref_counted(controller_ptr) }
            .expect("Failed to create a group controller");

        // Only cache while there's a live pipe, because closing the pipe is the
        // only thing that breaks the cycle between the shared state and the
        // controller. A router with no pipe at all is only used in tests.
        if !shared_state.pipe_closed && self.endpoint_watcher.to_arc().is_some() {
            shared_state.cpp_group_controller = Some(controller.clone());
        }

        controller
    }
}
