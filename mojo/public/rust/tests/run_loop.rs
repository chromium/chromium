// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests all functionality in the system package
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

use mojo::bindings::run_loop;
use mojo::bindings::run_loop::{Handler, RunLoop, Token, WaitError};
use mojo::system::{message_pipe, HandleSignals, MOJO_INDEFINITE};

use std::cell::Cell;
use std::rc::Rc;

struct HandlerExpectReady {}

impl Handler for HandlerExpectReady {
    fn on_ready(&mut self, runloop: &mut RunLoop, token: Token) {
        runloop.deregister(token);
    }
    fn on_timeout(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Timed-out when expected ready");
    }
    fn on_error(&mut self, _runloop: &mut RunLoop, _token: Token, _error: WaitError) {
        panic!("Error when expected ready");
    }
}

struct HandlerExpectTimeout {}

impl Handler for HandlerExpectTimeout {
    fn on_ready(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Ready when expected timeout");
    }
    fn on_timeout(&mut self, runloop: &mut RunLoop, token: Token) {
        runloop.deregister(token);
    }
    fn on_error(&mut self, _runloop: &mut RunLoop, _token: Token, _error: WaitError) {
        panic!("Error when expected timeout");
    }
}

struct HandlerExpectError {
    was_called: Rc<Cell<bool>>,
}

impl Handler for HandlerExpectError {
    fn on_ready(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Ready when expected error");
    }
    fn on_timeout(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Timed-out when expected error");
    }
    fn on_error(&mut self, runloop: &mut RunLoop, token: Token, error: WaitError) {
        assert_eq!(error, WaitError::Unsatisfiable);
        runloop.deregister(token);
        self.was_called.set(true);
    }
}

struct HandlerQuit {}

impl Handler for HandlerQuit {
    fn on_ready(&mut self, runloop: &mut RunLoop, _token: Token) {
        runloop.quit();
    }
    fn on_timeout(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Timed-out when expected error");
    }
    fn on_error(&mut self, _runloop: &mut RunLoop, _token: Token, error: WaitError) {
        assert_eq!(error, WaitError::HandleClosed);
    }
}

struct HandlerRegister {}

impl Handler for HandlerRegister {
    fn on_ready(&mut self, runloop: &mut RunLoop, token: Token) {
        let (_, endpt1) = message_pipe::create().unwrap();
        let _ = runloop.register(
            &endpt1,
            HandleSignals::WRITABLE,
            MOJO_INDEFINITE,
            HandlerDeregisterOther { other: token },
        );
    }
    fn on_timeout(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Timed-out when expected error");
    }
    fn on_error(&mut self, _runloop: &mut RunLoop, _token: Token, _error: WaitError) {
        panic!("Error when expected ready");
    }
}

struct HandlerDeregisterOther {
    other: Token,
}

impl Handler for HandlerDeregisterOther {
    fn on_ready(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Ready when expected error");
    }
    fn on_timeout(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Timed-out when expected error");
    }
    fn on_error(&mut self, runloop: &mut RunLoop, token: Token, error: WaitError) {
        assert_eq!(error, WaitError::HandleClosed);
        runloop.deregister(token);
        runloop.deregister(self.other.clone());
    }
}

struct HandlerReregister {
    count: u64,
}

impl Handler for HandlerReregister {
    fn on_ready(&mut self, runloop: &mut RunLoop, token: Token) {
        runloop.deregister(token);
    }
    fn on_timeout(&mut self, runloop: &mut RunLoop, token: Token) {
        if self.count < 10 {
            runloop.reregister(&token, HandleSignals::READABLE, 0);
            self.count += 1;
        } else {
            runloop.reregister(&token, HandleSignals::WRITABLE, MOJO_INDEFINITE);
        }
    }
    fn on_error(&mut self, _runloop: &mut RunLoop, _token: Token, _error: WaitError) {
        panic!("Error when expected ready");
    }
}

struct HandlerNesting {
    count: u64,
}

impl Handler for HandlerNesting {
    fn on_ready(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Ready when expected timeout");
    }
    fn on_timeout(&mut self, runloop: &mut RunLoop, token: Token) {
        let mut nested_runloop = run_loop::RunLoop::new();
        if self.count < 10 {
            let handler = HandlerNesting { count: self.count + 1 };
            let (endpt0, _endpt1) = message_pipe::create().unwrap();
            let _ = nested_runloop.register(&endpt0, HandleSignals::READABLE, 0, handler);
            nested_runloop.run();
        } else {
            let handler = HandlerNesting { count: self.count + 1 };
            let (endpt0, _) = message_pipe::create().unwrap();
            let _ = nested_runloop.register(&endpt0, HandleSignals::READABLE, 0, handler);
            nested_runloop.run();
        }
        runloop.deregister(token);
    }
    fn on_error(&mut self, runloop: &mut RunLoop, token: Token, error: WaitError) {
        assert_eq!(error, WaitError::Unsatisfiable);
        assert_eq!(self.count, 11);
        runloop.deregister(token);
    }
}

struct HandlerBadNesting {}

impl Handler for HandlerBadNesting {
    fn on_ready(&mut self, runloop: &mut RunLoop, _token: Token) {
        runloop.quit();
    }
    fn on_timeout(&mut self, runloop: &mut RunLoop, _token: Token) {
        runloop.run();
    }
    fn on_error(&mut self, runloop: &mut RunLoop, _token: Token, _error: WaitError) {
        runloop.quit();
    }
}

struct HandlerTasks {
    count: Rc<Cell<u64>>,
}

impl Handler for HandlerTasks {
    fn on_ready(&mut self, runloop: &mut RunLoop, token: Token) {
        let r = self.count.clone();
        let _ = runloop.post_task(
            move |_runloop| {
                let val = (*r).get();
                (*r).set(val + 1);
            },
            10,
        );
        if (*self.count).get() > 10 {
            runloop.deregister(token);
        }
    }
    fn on_timeout(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Timed-out when expected error");
    }
    fn on_error(&mut self, _runloop: &mut RunLoop, _token: Token, _error: WaitError) {
        panic!("Error when expected ready");
    }
}

struct NestedTasks {
    count: Rc<Cell<u64>>,
    quitter: bool,
}

impl Handler for NestedTasks {
    fn on_ready(&mut self, runloop: &mut RunLoop, token: Token) {
        let r = self.count.clone();
        let quit = self.quitter;
        let _ = runloop.post_task(
            move |runloop| {
                let r2 = r.clone();
                if (*r).get() < 10 {
                    let _ = runloop.post_task(
                        move |_runloop| {
                            let val = (*r2).get();
                            (*r2).set(val + 1);
                        },
                        0,
                    );
                } else {
                    if quit {
                        runloop.quit();
                    } else {
                        runloop.deregister(token);
                    }
                }
            },
            0,
        );
    }
    fn on_timeout(&mut self, _runloop: &mut RunLoop, _token: Token) {
        panic!("Timed-out when expected error");
    }
    fn on_error(&mut self, _runloop: &mut RunLoop, _token: Token, error: WaitError) {
        assert_eq!(error, WaitError::HandleClosed);
    }
}

tests! {
    // Verifies that after adding and removing, we can run, exit and be
    // left in a consistent state.
    fn add_remove() {
        run_loop::with_current(|runloop| {
            let (endpt0, endpt1) = message_pipe::create().unwrap();
            let token0 = runloop.register(&endpt0, HandleSignals::WRITABLE, 0, HandlerExpectReady {});
            let token1 = runloop.register(&endpt1, HandleSignals::WRITABLE, 0, HandlerExpectReady {});
            runloop.deregister(token1);
            runloop.deregister(token0);
            runloop.run();
        })
    }

    // Verifies that generated tokens are unique.
    fn tokens() {
        let mut vec = Vec::new();
        run_loop::with_current(|runloop| {
            for _ in 0..10 {
                let (_endpt0, endpt1) = message_pipe::create().unwrap();
                vec.push(runloop.register(&endpt1, HandleSignals::empty(), 0, HandlerExpectReady {}));
            }
            for i in 0..10 {
                for j in 0..10 {
                    if i != j {
                        assert!(vec[i] != vec[j]);
                    }
                }
            }
        });
    }

    // Verifies that the handler's "on_ready" function is called.
    fn notify_results() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::WRITABLE, MOJO_INDEFINITE, HandlerExpectReady {});
            runloop.run();
        });
    }

    // Verifies that the handler's "on_error" function is called.
    fn notify_error() {
        // Drop the first endpoint immediately
        let (_, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let was_called = Rc::new(Cell::new(false));
            let _ = runloop.register(&endpt1, HandleSignals::READABLE, 0,
                HandlerExpectError {
                    was_called: was_called.clone(),
            });
            runloop.run();
            assert!(was_called.get(), "on_error was not called");
        });
    }

    // Verifies that the handler's "on_ready" function is called which only
    // quits.
    fn notify_ready_quit() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::WRITABLE, MOJO_INDEFINITE, HandlerQuit {});
            runloop.run();
        });
    }

    // Tests more complex behavior, i.e. the interaction between two handlers.
    fn register_deregister() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::WRITABLE, MOJO_INDEFINITE, HandlerRegister {});
            runloop.run();
        });
    }

    // Tests reregistering.
    #[ignore]
    fn reregister() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::READABLE, 0, HandlerReregister { count: 0 });
            runloop.run();
        });
    }

    // Tests nesting run loops by having a handler create a new one.
    #[ignore]
    fn nesting() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::READABLE, 0, HandlerNesting { count: 0 });
            runloop.run();
        });
    }

    // Tests to make sure nesting with the SAME runloop fails.
    #[should_panic]
    #[ignore]
    fn bad_nesting() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::READABLE, 0, HandlerBadNesting {});
            runloop.run();
        });
    }

    // Tests adding a simple task that adds a handler.
    fn simple_task() {
        run_loop::with_current(|runloop| {
            let was_called = Rc::new(Cell::new(false));
            // The inner closure cannot take `was_called` by reference since we
            // cannot prove it lives long enough to the borrow checker. It must
            // be cloned first.
            let was_called_clone = was_called.clone();
            let (_, endpt1) = message_pipe::create().unwrap();
            // If `endpt1` is moved into the task closure, it is dropped at the
            // end of its call. This means we won't get the signal we're looking
            // for: we'll just get a notification that the handle was closed.
            // Make it live longer by wrapping it in an Rc.
            let endpt1 = Rc::new(endpt1);
            let endpt1_clone = endpt1.clone();
            let _ = runloop.post_task(move |runloop: &mut RunLoop| {
                let _ = runloop.register(&*endpt1_clone, HandleSignals::READABLE, 0,
                    HandlerExpectError {
                        was_called: was_called_clone,
                    });
            }, 0).unwrap();
            runloop.run();

            // Ensure we got the `on_error` call for the unsatisfiable signal.
            assert!(was_called.get(), "on_error was not called");
        });
    }

    // Tests using a handler that adds a bunch of tasks.
    fn handler_tasks() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        let r = Rc::new(Cell::new(0));
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::WRITABLE, 0, HandlerTasks { count: r.clone() });
            runloop.run();
            assert!((*r).get() >= 11);
        });
    }

    // Tests using a handler that adds a bunch of tasks.
    fn nested_tasks() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        let r = Rc::new(Cell::new(0));
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::WRITABLE, 0, NestedTasks { count: r.clone(), quitter: false });
            runloop.run();
            assert!((*r).get() >= 10);
        });
    }

    // Tests using a handler that adds a bunch of tasks.
    //
    // This test is disabled because it posts tasks which call `RunLoop::quit`,
    // some of which get left on the `RunLoop` after the test ends. These
    // interfere with later tests on the same thread.
    #[ignore]
    fn nested_tasks_quit() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        let r = Rc::new(Cell::new(0));
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::WRITABLE, 0, NestedTasks { count: r.clone(), quitter: true });
            runloop.run();
            assert!((*r).get() >= 10);
        });
    }

    fn close_handle() {
        let (_endpt0, endpt1) = message_pipe::create().unwrap();
        run_loop::with_current(|runloop| {
            let _ = runloop.register(&endpt1, HandleSignals::WRITABLE, 0, HandlerQuit {});
            drop(endpt1);
            runloop.run();
        })
    }

    // Tests that `RunLoop::run` will run posted tasks even if there's no
    // handles to wait on.
    fn post_tasks_without_handles() {
        let outer_called = Rc::new(Cell::new(false));
        let outer_called_clone = outer_called.clone();
        let inner_called = Rc::new(Cell::new(false));
        let inner_called_clone = inner_called.clone();

        run_loop::with_current(move |runloop| {
            // Post a task, and then post another task from within the first.
            // This is to check RunLoop will run all the tasks it can before
            // quitting.
            runloop.post_task(move |runloop| {
                outer_called_clone.set(true);
                runloop.post_task(move |_| {
                    inner_called_clone.set(true);
                }, 0).unwrap();
            }, 0).unwrap();
            runloop.run();
        });

        assert!(outer_called.get());
        assert!(inner_called.get());
    }
}
