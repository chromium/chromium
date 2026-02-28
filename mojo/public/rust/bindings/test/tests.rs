// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Arc, Mutex};

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/bindings/test:bindings_unittests_mojom_rust";
    "//mojo/public/rust/system";
    "//mojo/public/rust/system/test_util";
    "//mojo/public/rust/sequences";
    "//mojo/public/rust/sequences:test_cxx";
}

use bindings::receiver::PendingReceiver;
use bindings::remote::PendingRemote;
use bindings_unittests_mojom_rust::bindings_unittests as test_mojom;
use sequences::run_loop::RunLoop;
use system::mojo_types::UntypedHandle;

use test_mojom::{HandleService, MathService, TwoInts};

use crate::state_objects::*;

#[gtest(RustBindingsAPI, MessagePipeWatcherBasicTests)]
fn test_watcher_basic() {
    // Exercise the various watcher methods so we have some coverage
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    use bindings::message_pipe_watcher::{MessagePipeWatcher, ResponseSender};
    use system::message::RawMojoMessage;
    use system::message_pipe::MessageEndpoint;

    let (sender, receiver) = MessageEndpoint::create_pipe().unwrap();

    let received_messages: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(Vec::new()));
    let received_messages_clone = Arc::clone(&received_messages);

    // When we receive a message at the receiver, store it in `received_messages`
    // and send a simple response.
    let receiver_msg_handler = move |raw_msg: RawMojoMessage, sender: ResponseSender| {
        let msg_contents = String::from_utf8(raw_msg.read_bytes().unwrap().to_vec()).unwrap();
        received_messages_clone.lock().unwrap().push(msg_contents);
        sender.try_send_response(RawMojoMessage::new_with_bytes(b"Got it!").unwrap());
    };

    let run_loop = RunLoop::new();
    let quit_loop = run_loop.get_quit_closure();

    let mut response_count = 0;

    // When we get a response, just increment a counter. Once we've gotten three,
    // all messages have arrived, so stop running tasks
    let sender_msg_handler = move |_, _| {
        response_count += 1;
        if response_count >= 3 {
            quit_loop();
        }
    };

    let sender = MessagePipeWatcher::new(sender, sender_msg_handler, None).unwrap();
    let receiver = MessagePipeWatcher::new(receiver, receiver_msg_handler, None).unwrap();

    // Send some messages through; this should trigger the handler twice
    sender.send_message(RawMojoMessage::new_with_bytes(b"Message 1").unwrap()).unwrap();
    sender.send_message(RawMojoMessage::new_with_bytes(b"Message 2").unwrap()).unwrap();
    // Send a message the other way and make sure it arrived
    receiver.send_message(RawMojoMessage::new_with_bytes(b"From Receiver").unwrap()).unwrap();

    run_loop.run();

    let _sender = sender.into_endpoint();

    // Make sure we got 2 (the system handler ensures we got 3 responses)
    expect_eq!(2, received_messages.lock().unwrap().len());
}

#[gtest(RustBindingsAPI, MessagePipeWatcherDisconnectTests)]
fn test_watcher_disconnect() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    use bindings::message_pipe_watcher::MessagePipeWatcher;
    use system::message_pipe::MessageEndpoint;

    let (sender, receiver) = MessageEndpoint::create_pipe().unwrap();

    let receiver_msg_handler = |_, _| {};

    let run_loop = RunLoop::new();
    let quit_loop = run_loop.get_quit_closure();

    // When we get a disconnect notification, we're done for the day
    let receiver_disconnect_handler = move || quit_loop();

    let receiver = MessagePipeWatcher::new(
        receiver,
        receiver_msg_handler,
        Some(Box::new(receiver_disconnect_handler)),
    )
    .unwrap();

    expect_true!(receiver.is_connected());

    // This will send a disconnect notification
    drop(sender);

    // Wait for it...
    run_loop.run();

    // Now we're no longer connected.
    expect_false!(receiver.is_connected());
}

#[gtest(RustBindingsAPI, MessagePipeWatcherDisconnectImmediatelyTests)]
fn test_watcher_disconnect_immediately() {
    // Make sure things work fine if a watcher is constructed with an endpoint
    // whose counterpoint has already been dropped.
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    use bindings::message_pipe_watcher::MessagePipeWatcher;
    use system::message_pipe::MessageEndpoint;

    let (sender, receiver) = MessageEndpoint::create_pipe().unwrap();

    let ran_disconnect_handler = Arc::new(Mutex::new(false));
    let ran_disconnect_handler_clone = Arc::clone(&ran_disconnect_handler);

    let run_loop = RunLoop::new();
    let quit_loop = run_loop.get_quit_closure();

    let receiver_disconnect_handler = move || {
        *ran_disconnect_handler_clone.lock().unwrap() = true;
        quit_loop();
    };

    drop(sender);
    let receiver =
        MessagePipeWatcher::new(receiver, |_, _| {}, Some(Box::new(receiver_disconnect_handler)))
            .unwrap();

    run_loop.run();

    expect_false!(receiver.is_connected());
    expect_true!(*ran_disconnect_handler.lock().unwrap());
}

#[gtest(RustBindingsAPI, RemoteReceiverWrapMathTest)]
fn test_remote_receiver_wrapping() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    let mut wrapping_remote = pending_remote.bind();
    let _wrapping_receiver = pending_receiver.bind(WrappingMathService {});

    // Send some messages!
    wrapping_remote.Add(1, 2, |n| expect_eq!(n, 3));
    wrapping_remote.Add(1, u32::MAX, |n| expect_eq!(n, 0));
    wrapping_remote.AddTwoInts(TwoInts { a: 7, b: 12 }, move |n| {
        expect_eq!(n, 19);
        quit();
    });

    run_loop.run();
}

#[gtest(RustBindingsAPI, RemoteReceiverSatMathTest)]
fn test_remote_receiver_saturating() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    // We can use the same constructor call as in the previous test,
    // but for a different type of state object!
    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    let mut saturating_remote = pending_remote.bind();
    let _saturating_receiver = pending_receiver.bind(SaturatingMathService {});

    // Send some more messages!
    saturating_remote.Add(1, 2, |n| expect_eq!(n, 3));
    saturating_remote.Add(1, u32::MAX, |n| expect_eq!(n, u32::MAX)); // Saturating!
    saturating_remote.AddTwoInts(TwoInts { a: 7, b: 12 }, move |n| {
        expect_eq!(n, 19);
        quit();
    });

    run_loop.run();
}

#[gtest(RustBindingsAPI, RemoteReceiverNotifMathTest)]
fn test_remote_receiver_notifying() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    // Tracks the sum of all things we've added together
    let sum = Arc::new(Mutex::new(0));
    let sum_clone = Arc::downgrade(&sum);

    let add_to_sum = move |n| *sum_clone.upgrade().unwrap().try_lock().unwrap() += n;

    let mut notifying_remote = pending_remote.bind();
    let _notifying_receiver = pending_receiver.bind(NotifyingMathService { f: add_to_sum });

    // Send some more messages!
    notifying_remote.Add(1, 2, |n| expect_eq!(n, 3));
    notifying_remote.Add(4, 5, |n| expect_eq!(n, 9));
    notifying_remote.AddTwoInts(TwoInts { a: 7, b: 12 }, move |n| {
        expect_eq!(n, 19);
        quit();
    });

    run_loop.run();

    // 1 + 2 + 3 + 4 + 7 + 12 = 31
    expect_eq!(Arc::into_inner(sum).unwrap().into_inner().unwrap(), 31);
}

#[gtest(RustBindingsAPI, CppReceiverTest)]
fn test_cpp_receiver() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    // Pass the receiver handle to C++ and bind it there
    let receiver_handle = UntypedHandle::from(pending_receiver.into_endpoint()).into_raw_value();
    let _cpp_receiver = crate::cxx::ffi::CreatePlusSevenMathService(receiver_handle);

    let mut remote = pending_remote.bind();

    // These message must have been processed in C++ because the C++
    // implementation is the only one that adds 7 to all its results!
    remote.Add(1, 2, |n| expect_eq!(n, 10));
    remote.Add(10, 20, |n| expect_eq!(n, 37));
    remote.AddTwoInts(TwoInts { a: 100, b: 50 }, move |n| {
        expect_eq!(n, 157);
        quit();
    });

    run_loop.run();
}

#[gtest(RustBindingsAPI, CppRemoteTest)]
fn test_cpp_remote() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    // Track the running sum of all additions we perform.
    let sum = Arc::new(Mutex::new(0));
    let sum_clone = Arc::clone(&sum);

    let notifying_service = NotifyingMathService {
        f: move |n| {
            let mut sum = sum_clone.lock().unwrap();
            *sum += n;
        },
    };

    let _receiver = pending_receiver.bind(notifying_service);

    // Pass the remote handle to C++ and have it send messages.
    // This call blocks until all responses are received.
    let remote_handle = UntypedHandle::from(pending_remote.into_endpoint()).into_raw_value();
    crate::cxx::ffi::TestRemoteFromCpp(remote_handle);

    // These message must have come from C++ because `TestFromRemote` i
    // the only testing function that adds things to a total of 22!
    expect_eq!(*sum.lock().unwrap(), 22);
}

/// This function tests that we can send handles via mojom messages and then
/// successfully use them afterwards (so they didn't get corrupted, re-ordered,
/// etc. during transit).
///
/// This test is meant to compensate for the fact that we can't compare handles
/// directly; we get some confidence that we're passing them correctly because
/// afterwards, they work as expected.
#[gtest(RustBindingsAPI, HandlePassingTest)]
fn test_handle_passing() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let (handle_pending_remote, handle_pending_receiver) =
        PendingRemote::<dyn test_mojom::HandleService>::new_pipe().unwrap();
    let mut handle_remote = handle_pending_remote.bind();

    // Make a bunch of pairs so there's some room for things to go wrong
    let (math_remote1, math_receiver1) = PendingRemote::<dyn MathService>::new_pipe().unwrap();
    let (math_remote2, math_receiver2) = PendingRemote::<dyn MathService>::new_pipe().unwrap();
    let (math_remote3, math_receiver3) = PendingRemote::<dyn MathService>::new_pipe().unwrap();
    let (math_remote4, math_receiver4) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    let mut math_remote1 = math_remote1.bind();
    let mut math_remote2 = math_remote2.bind();
    let mut math_remote3 = math_remote3.bind();
    let mut math_remote4 = math_remote4.bind();

    // Treat math_receiver4 as an untyped handle for the purpose of the test.
    let h4 = UntypedHandle::from(math_receiver4.into_endpoint());

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    let math_receivers = Arc::new(Mutex::new(Vec::new()));
    let math_receivers_clone = Arc::clone(&math_receivers);

    // When we receive a message with the 4 handles, bind each of them to a
    // NotifyingMathService that expects to get a specific result each time
    // it's called. This lets us ensure that our messages are going to the
    // _right_ receiver.
    let handle_service = HandleServiceImpl {
        f: move |h1, h2, h3, h4| {
            let mut recvs = math_receivers_clone.lock().unwrap();
            // We use a helper closure: Bind the given handle and set it to
            // expect a result of `expected`.
            let mut bind = |h, expected: u32| {
                let service = NotifyingMathService {
                    f: Box::new(move |n| expect_eq!(n, expected)) as Box<dyn FnMut(u32) + Send>,
                };
                recvs.push(PendingReceiver::<dyn MathService>::new(h).bind(service));
            };
            bind(h1, 2);
            bind(h2, 4);
            bind(h3, 6);
            bind(h4.into(), 8);
        },
    };

    let _handle_receiver = handle_pending_receiver.bind(handle_service);

    handle_remote.PassHandles(
        math_receiver1.into_endpoint(),
        math_receiver2.into_endpoint(),
        math_receiver3.into_endpoint(),
        h4,
    );

    let responses_received = Arc::new(Mutex::new(0));
    let responses_received_clone = Arc::clone(&responses_received);
    let quit_arc = Arc::new(quit);

    let send_request = |remote: &mut bindings::remote::Remote<dyn MathService>, expected| {
        let responses_received_inner = Arc::clone(&responses_received_clone);
        let quit_inner = Arc::clone(&quit_arc);
        remote.Add(expected, 0, move |n| {
            expect_eq!(expected, n);

            // Quit the run loop after we've gotten all 4 responses
            let mut count = responses_received_inner.lock().unwrap();
            *count += 1;
            if *count == 4 {
                (quit_inner)();
            }
        });
    };

    send_request(&mut math_remote1, 2);
    send_request(&mut math_remote2, 4);
    send_request(&mut math_remote3, 6);
    send_request(&mut math_remote4, 8);

    run_loop.run();

    expect_eq!(*responses_received.lock().unwrap(), 4);
}

#[gtest(RustBindingsAPI, DisconnectHandlersTest)]
fn test_disconnect_handlers() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    // Test Receiver disconnect handler
    let run_loop = RunLoop::new();
    let quit_loop = run_loop.get_quit_closure();

    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();
    let _receiver = pending_receiver.bind_with_options(
        SaturatingMathService {},
        None,
        Some(Box::new(quit_loop)),
    );
    drop(pending_remote);

    run_loop.run();

    // Test Remote disconnect handler
    let run_loop = RunLoop::new();
    let quit_loop = run_loop.get_quit_closure();

    // Test Receiver disconnect handler
    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();
    let _remote = pending_remote.bind_with_options(None, Some(Box::new(quit_loop)));
    drop(pending_receiver);

    run_loop.run();
}

#[gtest(RustBindingsAPI, SelfOwnedReceiverTest)]
fn test_self_owned_receiver() {
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    let run_loop = RunLoop::new();
    let quit_loop = Box::new(run_loop.get_quit_closure());

    let dropped = Arc::new(Mutex::new(false));
    let dropped_clone = Arc::clone(&dropped);

    let service = DropNotifyingService { dropped, quit_loop };

    // Create a self-owned receiver.
    let self_owned = pending_receiver.bind_self_owned(service);

    // Disconnect the pipe. This should trigger the disconnect handler, which
    // will drop the receiver, which will drop the service.
    drop(pending_remote);

    run_loop.run();

    expect_true!(*dropped_clone.lock().unwrap());
    expect_true!(self_owned.upgrade().is_none());
}
