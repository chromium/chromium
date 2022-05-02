// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains a thread-local run-loop.
//!
//! The run-loop may have handles and handlers pre-registers
//! (and in fact, must) in order to keep running. The run-loop
//! executes until it has no more handles or handlers on itself,
//! or until it is told to quit via stop().
//!
//! The run-loop waits until some signals on some handle is satisfied,
//! at which point it wakes up and executes the appropriate handler
//! method. This handler method may then be used to further populate
//! or de-populate the run-loop.
//!
//! As of yet, the run-loop is NOT thread-safe. Although it is useful
//! to be able to register tasks or handles from one thread onto
//! another thread's run-loop, this is as-of-yet unsupported, and
//! Rust should complain loudly when you try to do any threading here.

use std::cell::RefCell;
use std::cmp::{Eq, Ord, Ordering, PartialEq, PartialOrd};
use std::collections::BinaryHeap;
use std::collections::HashMap;
use std::i64;
use std::u32;
use std::vec::Vec;

use crate::system;
use crate::system::core;
use crate::system::wait_set;
use crate::system::{Handle, MojoResult, MOJO_INDEFINITE};

/// Define the equivalent of MOJO_INDEFINITE for absolute deadlines
const MOJO_INDEFINITE_ABSOLUTE: system::MojoTimeTicks = 0;

// TODO(mknyszek): The numbers below are arbitrary and come from the C++ bindings,
// and should probably be changed at some point

/// Initial size of the result buffer.
const INITIAL_WAIT_SET_NUM_RESULTS: usize = 16;

/// Maximum size of the result buffer.
const MAXIMUM_WAIT_SET_NUM_RESULTS: usize = 256;

// Thread-local data structure for keeping track of handles to wait on.
thread_local!(static TL_RUN_LOOP: RefCell<RunLoop<'static, 'static>> = RefCell::new(RunLoop::new()));

/// Token representing handle/callback to wait on for this thread only. This
/// token only has meaning on the thread in which the handle was registered.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Token(u64);

impl Token {
    /// Get the wait token's "cookie" form, suitable for use in a wait set.
    fn as_cookie(&self) -> u64 {
        self.0
    }
}

/// Represents the possible error cases that may occur when waiting
/// on a handle in a RunLoop.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum WaitError {
    /// The handle has been closed or is otherwise no longer valid.
    HandleClosed,

    /// The handle is currently busy in some transaction.
    HandleBusy,

    /// It has been determined that the signals provided will never
    /// be satisfied for this handle.
    Unsatisfiable,
}

/// A trait which defines an interface to be a handler usable by
/// a RunLoop.
pub trait Handler {
    /// Called after a successful wait.
    fn on_ready(&mut self, runloop: &mut RunLoop, token: Token);

    /// Called after the given deadline expires.
    fn on_timeout(&mut self, runloop: &mut RunLoop, token: Token);

    /// Called when an unexpected error occurs.
    fn on_error(&mut self, runloop: &mut RunLoop, token: Token, error: WaitError);
}

/// A wrapper struct for carrying the handler as well as various information
/// about it.
struct HandlerInfo<'h> {
    /// The handle for which we are waiting.
    ///
    /// We keep this handle around so that we may easily re-register.
    handle: system::MojoHandle,

    /// The handler, boxed up.
    ///
    /// The handler is in an Option type because if it is currently being
    /// used in a callback, we must take ownership to avoid mutability
    /// cycles. The easiest way to do this is to take() from the Option then
    /// put it back.
    handler: Option<Box<dyn Handler + 'h>>,

    /// An absolute deadline in terms of time ticks.
    ///
    /// This is the most recently updated deadline that
    /// we should be watching out for. All others for this
    /// token may be considered "stale".
    deadline: system::MojoTimeTicks,
}

impl<'h> HandlerInfo<'h> {
    /// Take the handler out of its Option type.
    pub fn take(&mut self) -> Option<Box<dyn Handler + 'h>> {
        self.handler.take()
    }

    /// Put a new handler into the Option type.
    pub fn give(&mut self, handler: Box<dyn Handler + 'h>) {
        self.handler = Some(handler);
    }

    /// Getter for the system::MojoHandle held inside.
    pub fn handle(&self) -> system::MojoHandle {
        self.handle
    }

    /// Getter for the current absolute deadline held inside.
    pub fn deadline(&self) -> system::MojoTimeTicks {
        self.deadline
    }

    /// Setter to update the current absolute deadline.
    pub fn set_deadline(&mut self, deadline: system::MojoTimeTicks) {
        self.deadline = deadline
    }
}

/// A wrapper struct for carrying the task as well as some information about
/// it.
struct TaskInfo<'t> {
    /// The task, boxed up.
    closure: Box<dyn FnMut(&mut RunLoop) + 't>,

    /// An absolute deadline in terms of time ticks.
    ///
    /// This is the most recently updated deadline that
    /// we should be watching out for. All others for this
    /// token may be considered "stale".
    deadline: system::MojoTimeTicks,
}

impl<'t> TaskInfo<'t> {
    /// Executes the task within the info object, consuming it
    /// in the process.
    pub fn execute_task(mut self, run_loop: &mut RunLoop) {
        (*self.closure)(run_loop);
    }

    /// Getter for the current absolute deadline held inside.
    pub fn deadline(&self) -> system::MojoTimeTicks {
        self.deadline
    }
}

impl<'t> PartialEq for TaskInfo<'t> {
    /// Equality for TaskInfo in terms of its deadline
    fn eq(&self, other: &TaskInfo) -> bool {
        self.deadline == other.deadline
    }
}

impl<'t> Eq for TaskInfo<'t> {}

impl<'t> PartialOrd for TaskInfo<'t> {
    /// Partial comparison for TaskInfo in terms of its deadline
    ///
    /// Reverses the comparison because the Rust std library only
    /// offers a max-heap, and we need a min-heap.
    fn partial_cmp(&self, other: &TaskInfo) -> Option<Ordering> {
        other.deadline.partial_cmp(&self.deadline)
    }
}

impl<'t> Ord for TaskInfo<'t> {
    /// Implement comparisons for Task Info.
    ///
    /// Reverses the comparison because the Rust std library only
    /// offers a max-heap, and we need a min-heap.
    fn cmp(&self, other: &Self) -> Ordering {
        other.deadline.cmp(&self.deadline)
    }
}

/// Wrapper struct intended to be used in a priority queue
/// for efficiently retrieving the next closest deadline.
#[derive(Clone)]
struct DeadlineInfo {
    /// The ID of the associated Handler struct in the RunLoop's
    /// hash map.
    token: Token,

    /// An absolute deadline in terms of time ticks.
    deadline: system::MojoTimeTicks,
}

impl DeadlineInfo {
    /// Getter for an immutable borrow for the token inside.
    pub fn token(&self) -> &Token {
        &self.token
    }

    /// Getter for the absolute deadline inside.
    pub fn deadline(&self) -> system::MojoTimeTicks {
        self.deadline
    }
}

impl PartialEq for DeadlineInfo {
    /// Equality for DeadlineInfo in terms of its deadline
    fn eq(&self, other: &DeadlineInfo) -> bool {
        self.deadline == other.deadline
    }
}

impl Eq for DeadlineInfo {}

impl PartialOrd for DeadlineInfo {
    /// Partial comparison for DeadlineInfo in terms of its deadline
    ///
    /// Reverses the comparison because the Rust std library only
    /// offers a max-heap, and we need a min-heap.
    fn partial_cmp(&self, other: &DeadlineInfo) -> Option<Ordering> {
        other.deadline.partial_cmp(&self.deadline)
    }
}

impl Ord for DeadlineInfo {
    /// Implement comparisons for Deadline Info.
    ///
    /// Reverses the comparison because the Rust std library only
    /// offers a max-heap, and we need a min-heap.
    fn cmp(&self, other: &Self) -> Ordering {
        other.deadline.cmp(&self.deadline)
    }
}

/// Convert a mojo deadline (which is a relative deadline to "now") to
/// an absolute deadline based on time ticks.
fn absolute_deadline(deadline: system::MojoDeadline) -> system::MojoTimeTicks {
    if deadline == MOJO_INDEFINITE {
        return MOJO_INDEFINITE_ABSOLUTE;
    }
    let mut converted = MOJO_INDEFINITE_ABSOLUTE;
    let max_time_ticks = i64::MAX as system::MojoDeadline;
    if deadline <= max_time_ticks {
        let now = core::get_time_ticks_now();
        if deadline <= (max_time_ticks - (now as u64)) {
            converted = (deadline as system::MojoTimeTicks) + now
        }
    }
    converted
}

/// This structure contains all information necessary to wait on handles
/// asynchronously.
///
/// Ultimately, it should only be a singleton living in
/// thread-local storage.
pub struct RunLoop<'h, 't> {
    /// Running count of the next available token slot.
    token_count: u64,

    /// A map of handlers.
    ///
    /// TODO(mknyszek): Try out a Slab allocator instead of a hashmap.
    handlers: HashMap<Token, HandlerInfo<'h>>,

    /// A min-heap of delayed tasks in order to pull the soonest task to
    /// execute efficiently.
    tasks: BinaryHeap<TaskInfo<'t>>,

    /// A min-heap containing deadlines in order to pull out the soonest
    /// deadline efficiently.
    ///
    /// Warning: may contain "stale" deadlines which are not kept in the
    /// map!
    deadlines: BinaryHeap<DeadlineInfo>,

    /// The Mojo structure keeping track of handles and signals.
    ///
    /// This structure must be kept in sync with handlers.
    handle_set: wait_set::WaitSet,

    /// A flag that tells the RunLoop whether it should quit.
    should_quit: bool,

    /// A flag that indicates whether the RunLoop is running or now
    running: bool,
}

impl<'h, 't> RunLoop<'h, 't> {
    /// Create a new RunLoop.
    pub fn new() -> RunLoop<'h, 't> {
        RunLoop {
            token_count: 0,
            handlers: HashMap::new(),
            tasks: BinaryHeap::new(),
            deadlines: BinaryHeap::new(),
            handle_set: wait_set::WaitSet::new(wsflags!(Create::None)).unwrap(),
            should_quit: false,
            running: false,
        }
    }

    /// Generate a new Token for this RunLoop
    fn generate_token(&mut self) -> Token {
        self.token_count = self.token_count.wrapping_add(1);
        Token(self.token_count)
    }

    /// Adds a new entry to the runloop queue.
    pub fn register<H>(
        &mut self,
        handle: &dyn Handle,
        signals: system::HandleSignals,
        deadline: system::MojoDeadline,
        handler: H,
    ) -> Token
    where
        H: Handler + 'h,
    {
        let token = self.generate_token();
        let abs_deadline = absolute_deadline(deadline);
        self.handle_set.add(handle, signals, token.as_cookie(), wsflags!(Add::None));
        self.deadlines.push(DeadlineInfo { token: token.clone(), deadline: abs_deadline });
        debug_assert!(!self.handlers.contains_key(&token));
        self.handlers.insert(
            token.clone(),
            HandlerInfo {
                handle: handle.get_native_handle(),
                handler: Some(Box::new(handler)),
                deadline: abs_deadline,
            },
        );
        token
    }

    /// Updates the signals and deadline of an existing handler in the
    /// runloop via token. The token remains unchanged and valid.
    ///
    /// Returns true on a successful update and false if the token was not
    /// found.
    pub fn reregister(
        &mut self,
        token: &Token,
        signals: system::HandleSignals,
        deadline: system::MojoDeadline,
    ) -> bool {
        match self.handlers.get_mut(&token) {
            Some(info) => {
                let _result = self.handle_set.remove(token.as_cookie());
                debug_assert_eq!(_result, MojoResult::Okay);
                let abs_deadline = absolute_deadline(deadline);
                // Update what deadline we should be looking for, rendering
                // all previously set deadlines "stale".
                info.set_deadline(abs_deadline);
                // Add a new deadline
                self.deadlines.push(DeadlineInfo { token: token.clone(), deadline: abs_deadline });
                // Acquire the raw handle held by the HandlerInfo in order to
                // call the wait_set's add method. Invalidate it immediately after
                // in order to prevent the handle from being closed.
                //
                // It's perfectly okay for the handle to be invalid, so although this
                // is all unsafe, the whole system should just call the handler with an
                // error.
                let mut dummy = unsafe { system::acquire(info.handle()) };
                self.handle_set.add(&dummy, signals, token.as_cookie(), wsflags!(Add::None));
                dummy.invalidate();
                true
            }
            None => false,
        }
    }

    /// Removes an entry from the runloop.
    ///
    /// Since we cannot remove from the deadlines heap, we just leave the deadline
    /// in there as "stale", and we handle those when trying to find the next closest
    /// deadline.
    pub fn deregister(&mut self, token: Token) -> bool {
        match self.handlers.remove(&token) {
            Some(_) => {
                let _result = self.handle_set.remove(token.as_cookie());
                // Handles are auto-removed if they are closed. Ignore this error.
                debug_assert!(_result == MojoResult::Okay || _result == MojoResult::NotFound);
                true
            }
            None => false,
        }
    }

    /// Adds a task to be run by the runloop after some delay.
    ///
    /// Returns a token if the delay is valid, otherwise returns None.
    pub fn post_task<F>(&mut self, task: F, delay: system::MojoTimeTicks) -> Result<(), ()>
    where
        F: FnMut(&mut RunLoop) + 't,
    {
        let now = core::get_time_ticks_now();
        if delay > i64::MAX - now {
            return Err(());
        }
        let deadline = now + delay;
        self.tasks.push(TaskInfo { closure: Box::new(task), deadline: deadline });
        Ok(())
    }

    /// Uses the binary heaps to get the next closest deadline.
    ///
    /// Removes stale deadline entries as they are found, but
    /// does not otherwise modify the heap of deadlines.
    fn get_next_deadline(&mut self) -> system::MojoTimeTicks {
        debug_assert!(!self.handlers.is_empty());
        let top_task_deadline = match self.tasks.peek() {
            Some(info) => info.deadline(),
            None => MOJO_INDEFINITE_ABSOLUTE,
        };
        let mut top = match self.deadlines.peek() {
            Some(info) => info.clone(),
            None => return MOJO_INDEFINITE_ABSOLUTE,
        };
        while !self.handlers.contains_key(top.token()) {
            self.deadlines.pop();
            top = match self.deadlines.peek() {
                Some(info) => info.clone(),
                None => return MOJO_INDEFINITE_ABSOLUTE,
            }
        }
        if top_task_deadline != MOJO_INDEFINITE_ABSOLUTE && top_task_deadline < top.deadline() {
            top_task_deadline
        } else {
            top.deadline()
        }
    }

    /// Gets a handler by token to be manipulated in a consistent environment.
    ///
    /// This method provides a method of accessing a handler in order to manipulate
    /// it in a manner that avoids a borrow cycle, that is, it take()s the handler
    /// out of the HashMap, and returns it when manipulation has completed.
    fn get_handler_with<F>(&mut self, token: &Token, invoker: F)
    where
        F: FnOnce(&mut Self, &mut Box<dyn Handler + 'h>, Token, system::MojoTimeTicks),
    {
        // Logic for pulling out the handler as well as its current deadline.
        //
        // Unfortunately, pulling out the handler value here and "onto the stack"
        // (it probably won't actually end up on the stack thanks to optimizations)
        // is necessary since otherwise the borrow checker complains that we pass
        // a mutable reference to the RunLoop and the handler (as &mut self) to
        // the callbacks at the same time. This is understandably unsafe since
        // modifying the hashmap with register and deregister can invalidate the
        // reference to self in the callback. In the C++ bindings and in other Rust
        // event loops this is exactly what happens, but I avoided this. The downside
        // is that we can no longer nest event loop run() calls. Once we put a handler
        // onto the stack here, we can no longer call its callback in a nested manner
        // from the RunLoop. I could just enable nesting with this one restriction, that
        // the handler calling run() will always be ignored, but this is unintuitive.
        let (mut handler, deadline) = match self.handlers.get_mut(&token) {
            Some(ref_info) => (
                match ref_info.take() {
                    Some(handler) => handler,
                    None => return,
                },
                ref_info.deadline(),
            ),
            None => return,
        };
        // Call the closure that will invoke the callbacks.
        invoker(self, &mut handler, token.clone(), deadline);
        // Restore the handler to its location in the HashMap
        if let Some(ref_info) = self.handlers.get_mut(&token) {
            ref_info.give(handler);
        }
    }

    /// For all the results we received, we notify the respective handle
    /// owners of the results by calling their given callbacks.
    ///
    /// We do NOT dequeue notified handles.
    fn notify_of_results(&mut self, results: &Vec<system::WaitSetResult>) {
        debug_assert!(!self.handlers.is_empty());
        for wsr in results.iter() {
            let token = Token(wsr.cookie());
            self.get_handler_with(&token, move |runloop, boxed_handler, token, _dl| {
                let handler = boxed_handler.as_mut();
                match wsr.result() {
                    MojoResult::Okay => handler.on_ready(runloop, token),
                    MojoResult::Cancelled => {
                        handler.on_error(runloop, token, WaitError::HandleClosed)
                    }
                    MojoResult::Busy => handler.on_error(runloop, token, WaitError::HandleBusy),
                    MojoResult::FailedPrecondition => {
                        handler.on_error(runloop, token, WaitError::Unsatisfiable)
                    }
                    other => panic!("Unexpected result received after waiting: {}", other),
                }
            });
            // In order to quit as soon as possible, we should check to quit after every
            // potential handler call, as any of them could have signaled to quit.
            if self.should_quit {
                break;
            }
        }
    }

    /// Since the deadline expired, we notify the relevant handle
    /// owners of the expiration by calling their given callbacks.
    ///
    /// We do NOT dequeue notified handles.
    fn notify_of_expired(&mut self, expired_deadline: system::MojoTimeTicks) {
        debug_assert!(!self.handlers.is_empty());
        let mut top = match self.deadlines.peek() {
            Some(info) => info.clone(),
            None => panic!("Should not be in notify_of_expired without at least one deadline!"),
        };
        while expired_deadline >= top.deadline() {
            let next_deadline = top.deadline();
            self.get_handler_with(
                top.token(),
                move |runloop, boxed_handler, token, expected_dl| {
                    let handler = boxed_handler.as_mut();
                    if next_deadline == expected_dl {
                        handler.on_timeout(runloop, token);
                    }
                },
            );
            // In order to quit as soon as possible, we should check to quit after every
            // potential handler call, as any of them could have signaled to quit.
            if self.should_quit {
                break;
            }
            // Remove the deadline
            self.deadlines.pop();
            // Break if the next deadline has not yet expired.
            top = match self.deadlines.peek() {
                Some(info) => info.clone(),
                None => break,
            };
        }
    }

    /// Iterates through all tasks whose deadline has passed and executes
    /// them, consuming their information object.
    ///
    /// These tasks all have access to the RunLoop so that they may reschedule
    /// themselves or manipulate the RunLoop in some other way.
    fn execute_ready_tasks(&mut self) {
        let now = core::get_time_ticks_now();
        let mut deadline = match self.tasks.peek() {
            Some(info) => info.deadline(),
            None => return,
        };
        while deadline < now {
            let top = self.tasks.pop().expect("Sudden change to heap?");
            top.execute_task(self);
            if self.should_quit {
                return;
            }
            deadline = match self.tasks.peek() {
                Some(info) => info.deadline(),
                None => return,
            };
        }
    }

    /// Blocks on handle_set.wait_on_set using the information contained
    /// within itself.
    ///
    /// This method blocks for only as long as the shortest deadline among all
    /// handles this thread has registered. This method returns immediately as
    /// soon as any one handle has its signals satisfied, fails to ever have its
    /// signals satisfied, or reaches its deadline.
    fn wait(&mut self, results_buffer: &mut Vec<system::WaitSetResult>) {
        debug_assert!(!self.handlers.is_empty());
        self.execute_ready_tasks();
        // If after executing a task we quit or there are no handles,
        // we have no reason to continue.
        if self.handlers.is_empty() || self.should_quit {
            return;
        }

        let deadline = self.get_next_deadline();
        // Deadlines no longer supported. TODO(collinbaker): update run_loop
        // for no deadlines
        // let until_deadline = relative_deadline(deadline, core::get_time_ticks_now());
        // Perform the wait
        match self.handle_set.wait_on_set(results_buffer) {
            MojoResult::Okay => {
                let num_results = results_buffer.len();
                self.notify_of_results(results_buffer);

                // Clear the buffer since we don't need the results anymore.
                // Helps prevent a copy if we resize the buffer.
                results_buffer.clear();
                // If we reached the capcity of `results_buffer` there's a chance there were more handles available. Grow the buffer.
                let capacity = results_buffer.capacity();
                if capacity < MAXIMUM_WAIT_SET_NUM_RESULTS && capacity == num_results {
                    results_buffer.reserve(capacity);
                }
            }
            result => {
                assert_eq!(result, MojoResult::DeadlineExceeded);
                self.notify_of_expired(deadline);
            }
        }
    }

    /// Loop forever until a callback tells us to quit.
    pub fn run(&mut self) {
        // It's an error it already be running...
        if self.running {
            panic!("RunLoop is already running!");
        }
        self.running = true;
        self.should_quit = false;
        let mut buffer: Vec<system::WaitSetResult> =
            Vec::with_capacity(INITIAL_WAIT_SET_NUM_RESULTS);
        // Loop while we haven't been signaled to quit, and there's something to wait on.
        while !self.should_quit && !self.handlers.is_empty() {
            self.wait(&mut buffer)
        }
        self.running = false;
    }

    /// Set a flag to quit at the next available moment.
    pub fn quit(&mut self) {
        self.should_quit = true;
    }
}

/// Provides a scope to modify the current thread's runloop.
pub fn with_current<F>(modifier: F)
where
    F: FnOnce(&mut RunLoop),
{
    TL_RUN_LOOP.with(|ref_runloop| {
        let mut runloop = ref_runloop.borrow_mut();
        modifier(&mut *runloop);
    });
}
