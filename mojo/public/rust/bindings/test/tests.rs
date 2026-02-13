// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Arc, Mutex};

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/bindings:bindings_unittests_mojom_rust";
    "//mojo/public/rust/system";
    "//mojo/public/rust/system/test_util";
    "//mojo/public/rust/sequences";
    "//mojo/public/rust/sequences:test_cxx";
}

use bindings::remote::PendingRemote;
use bindings_unittests_mojom_rust::bindings_unittests as test_mojom;
use sequences::run_loop::RunLoop;

use test_mojom::{MathService, TwoInts};

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
