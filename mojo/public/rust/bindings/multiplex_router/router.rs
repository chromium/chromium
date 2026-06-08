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
  "//base:sequenced_task_runner";
}

use std::collections::{HashMap, VecDeque};
use std::sync::{Arc, Mutex, Weak};

use system::message_pipe::MessageEndpoint;

use super::arc_or_weak::ArcOrWeak;
use crate::message::MojomMessage;
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
    Disconnect(InterfaceId),
}

/// An object which is responsible for sending and receiving messages on a
/// single message pipe, ensuring that each reaches the sender's paired
/// endpoint.
///
/// This object is never held directly; instead, remotes and receivers hold a
/// `MultiplexRouterHandle`, which is analogous to an Arc, but with extra
/// functionality, including a drop handler and tracking the interface ID.
#[derive(Clone)]
pub(super) struct MultiplexRouter {
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
        endpoint_map.insert(PRIMARY_INTERFACE_ID, Some(endpoint_info));

        let shared_state = Arc::new(Mutex::new(MultiplexRouterSharedState {
            registry: EndpointRegistry::new(sets_high_bit, endpoint_map),
            unscheduled_tasks: VecDeque::new(),
            pipe_closed: false,
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
                    Some(Box::new(move || router_weak_clone.run_all_disconnect_handlers())),
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
        Self {
            endpoint_watcher: ArcOrWeak::Weak(Weak::new()),
            shared_state: Arc::new(Mutex::new(MultiplexRouterSharedState {
                registry: EndpointRegistry::new(sets_high_bit, HashMap::new()),
                unscheduled_tasks: VecDeque::new(),
                pipe_closed: false,
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
    /// registered with the router. Since each pair of endpoints should have a
    /// unique ID, this should only happen if we receive a malformed mojo
    /// message.
    ///
    /// Note: This function is only called for _associated_ endpoints, never the
    /// primary one (those use `new` instead).
    pub(super) fn add_associated_interface(
        &self,
        interface_id_opt: Option<InterfaceId>,
        endpoint_info: Option<EndpointInfo>,
    ) -> Option<InterfaceId> {
        // Will be initialized in the block below, while shared_state is locked.
        let interface_id;
        {
            let mut shared_state = self.shared_state.lock().unwrap();
            interface_id =
                interface_id_opt.unwrap_or_else(|| shared_state.registry.get_new_interface_id());

            let previous_entry =
                shared_state.registry.endpoint_map.insert(interface_id, endpoint_info);

            // We should never try to add an interface ID that already exists, since
            // each pair should be unique. The only way this can happen is if we
            // get a malicious mojo message.
            if let Some(previous_entry) = previous_entry {
                // Restore our map to its previous, good state.
                shared_state.registry.endpoint_map.insert(interface_id, previous_entry);
                return None;
            }

            // If the underlying message pipe has been disconnected, then we should
            // immediately schedule the disconnect handler for any new endpoints.
            // This is only possible if someone tries to send an associated remote/
            // receiver across a pipe that's already closed; if so, the other endpoint
            // will be registered, but no messages will ever arrive for it.
            if shared_state.pipe_closed {
                shared_state.unscheduled_tasks.push_back(Task::Disconnect(interface_id));
            }
        }

        self.schedule_all_possible_tasks();

        return Some(interface_id);
    }

    /// Indicate that the endpoint associated with `interface_id` has been bound
    /// to a sequence, and is ready to process messages on that sequence.
    ///
    /// Optionally the user may provide a disconnect handler, which will be run
    /// if the endpoint can no longer receive messages from the other side of
    /// the pipe.
    pub(super) fn bind_interface(&self, interface_id: InterfaceId, endpoint_info: EndpointInfo) {
        {
            let mut shared_state = self.shared_state.lock().unwrap();
            shared_state
                .registry
                .endpoint_map
                .get_mut(&interface_id)
                .map(|info_opt| *info_opt = Some(endpoint_info))
                .expect("bind_interface should only be called for real interface IDs");
            // If the router's underlying pipe has been disconnected, we should
            // immediately schedule the disconnect router for this endpoint. Note
            // that if any messages have already arrived for it, those messages will
            // be processed first.
            if shared_state.pipe_closed {
                shared_state.unscheduled_tasks.push_back(Task::Disconnect(interface_id));
            }
        }
        self.schedule_all_possible_tasks();
    }

    /// Send a message through the underlying pipe with the given interface ID.
    pub(super) fn send_message(&self, mut msg: MojomMessage, interface_id: InterfaceId) {
        // Don't bother sending the message if we've been disconnected.
        // Technically this is just an optimization, since incoming messages to
        // a disconnected ID will be ignored on the other side.
        if self.shared_state.lock().unwrap().registry.endpoint_map.contains_key(&interface_id) {
            msg.header.interface_id = interface_id;
            self.endpoint_watcher.with(|watcher| watcher.send_message(msg.into()));
        }
    }

    /// Tell the router that this end of an interface has been closed, so it
    /// should send a disconnect notification to the other end.
    ///
    /// If the interface ID is 0 (the primary interface), that means that the
    /// entire pipe is closed, so instead we send disconnect messages to all
    /// the _associated_ interfaces on this side. The other endpoint's router
    /// will handle the notification to the interfaces on the other side.
    pub(super) fn notify_dropped(&self, interface_id: InterfaceId) {
        let _ = self.shared_state.lock().unwrap().registry.endpoint_map.remove(&interface_id);
        if interface_id == 0 {
            // If the primary interface is being dropped, then we don't need to
            // notify it of anything, but we do need to alert all the associated
            // interfaces on this side that they've been disconnected.

            // Note that we just removed the entry for the primary ID, so this
            // will run all _other_ disconnect handlers.
            self.run_all_disconnect_handlers();
        } else {
            // TODO(crbug.com/517547791): Otherwise, we need to alert the other side that it
            // is now disconnected. Mojo handles this automatically for the
            // primary interface, but other interfaces need to send a special
            // control message. This isn't yet implemented, but we can't panic
            // here because this happens whenever stuff gets dropped, which
            // happens during tests.

            self.schedule_all_possible_tasks();
        }
    }

    /// Read an incoming message from the wire, and route it to the appropriate
    /// interface.
    ///
    /// This is the overall event handler for the `MultiplexRouter`, which is
    /// run whenever there's an incoming message.
    fn incoming_message_handler(
        &self,
        raw_message: system::message::RawMojoMessage,
        _sender: ResponseSender,
    ) {
        let Some(message) = MojomMessage::parse_raw_or_report_bad_message(raw_message) else {
            // If conversion failed then the header must have been malformed.
            // The bad message has already been reported, so nothing else to do.
            return;
        };

        // Add this message to the queue to be scheduled later. If the queue is empty,
        // we'll just schedule it immediately.
        self.shared_state.lock().unwrap().unscheduled_tasks.push_back(Task::Message(message));

        self.schedule_all_possible_tasks();
    }

    /// Schedule each queued task on its runner, until we find a task that can't
    /// be scheduled because it's not yet bound to a runner.
    ///
    /// Since we need to guarantee that incoming messages are scheduled in FIFO
    /// order, we can't just process all messages that have a runner; we need to
    /// stop whenever we hit one that we can't handle yet.
    fn schedule_all_possible_tasks(&self) {
        let mut shared_state_guard = self.shared_state.lock().unwrap();
        let shared_state = &mut *shared_state_guard;
        let registry = &mut shared_state.registry;
        let unscheduled_tasks = &mut shared_state.unscheduled_tasks;
        while let Some(task) = unscheduled_tasks.front() {
            let interface_id = match &task {
                Task::Disconnect(interface_id) => interface_id,
                Task::Message(message) => &message.header.interface_id,
            };

            let endpoint_info = match registry.endpoint_map.get(interface_id) {
                None => {
                    // If we failed to find an entry for this interface ID, it means it was
                    // dropped and removed itself from the map. In that case, we'll never be
                    // able to handle a task for this ID again, so remove it from the queue.
                    //
                    // Note that the message can't be for an ID which we haven't _yet_ registered,
                    // because you can't send messages until one side is bound to a pipe, and that
                    // process registers both sides.
                    unscheduled_tasks.pop_front();
                    continue;
                }
                Some(None) => {
                    // This ID had an entry in the map, but it hasn't been bound to anything yet.
                    // We can't handle this message until that happens. To preserve FIFO ordering,
                    // we can't process any later messages either, so we're done for now.
                    return;
                }
                Some(Some(endpoint_info)) => endpoint_info,
            };

            // If both the previous checks succeeded, this task can be scheduled now,
            // so remove it from the queue.
            let task = unscheduled_tasks.pop_front().unwrap();

            match task {
                Task::Disconnect(interface_id) => {
                    // If it's a disconnect notification, remove the entry from the registry (this
                    // is guaranteed to run after all messages for that endpoint, so no need to
                    // keep it around), and run the disconnect handler if one was provided.
                    let endpoint_info =
                        registry.endpoint_map.remove(&interface_id).unwrap().unwrap();
                    if let Some(disconnect_handler) = endpoint_info.disconnect_handler {
                        endpoint_info.runner.post_task(disconnect_handler);
                    }
                }
                Task::Message(message) => {
                    let handler_clone = endpoint_info.incoming_message_handler.clone();
                    let router_clone = self.clone();
                    let response_sender = super::ResponseSender {
                        interface_id: message.header.interface_id,
                        router: router_clone,
                    };
                    endpoint_info
                        .runner
                        .post_task(move || (*handler_clone)(message, response_sender));
                }
            };
        }
    }

    /// Schedule a task to run the disconnect handler for each registered
    /// interface.
    ///
    /// This is the overall disconnect handler for the router, which is run
    /// when the other endpoint is closed. Note that it does not remove entries
    /// from the registry, because there may still be messages that need to be
    /// handled before the disconnect handler runs.
    fn run_all_disconnect_handlers(&self) {
        {
            let mut shared_state_guard = self.shared_state.lock().unwrap();
            let shared_state = &mut *shared_state_guard;
            let registry = &mut shared_state.registry;
            let unscheduled_tasks = &mut shared_state.unscheduled_tasks;

            for (interface_id, info_opt) in registry.endpoint_map.iter() {
                if info_opt.is_some() {
                    unscheduled_tasks.push_back(Task::Disconnect(*interface_id));
                }
                // We'll schedule the notifications for unbound endpoints when
                // they get bound, by checking `pipe_closed`. Scheduling only
                // for bound endpoints lets us avoid blocking their DC handler
                // on another endpoint getting bound. This is allowed by the
                // FIFO ordering because we won't be getting any new messages
                // after this point since the pipe is closed.
            }

            shared_state.pipe_closed = true;
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
}
