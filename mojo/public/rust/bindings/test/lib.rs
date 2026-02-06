// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Arc, Mutex};

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/system";
    "//mojo/public/rust/system/test_util";
    "//mojo/public/rust/sequences";
    "//mojo/public/rust/sequences:test_cxx";
}

use sequences::run_loop::RunLoop;

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
