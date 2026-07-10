// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Arc, Mutex, OnceLock};

chromium::import! {
    "//mojo/public/rust/bindings";
    "//mojo/public/rust/bindings/test:bindings_unittests_mojom_rust";
    "//mojo/public/rust/system";
    "//mojo/public/rust/system/test_util";
    "//base:run_loop";
    "//base/test:task_environment";
    "//base:sequenced_task_runner";
}

use bindings::message::MojomMessage;
use bindings::message_header::{MessageHeader, MessageHeaderFlags};
use bindings::receiver::{PendingAssociatedReceiver, PendingReceiver, Receiver};
use bindings::remote::{PendingAssociatedRemote, PendingRemote, Remote};
use bindings_unittests_mojom_rust::bindings_unittests as test_mojom;
use run_loop::RunLoop;
use system::mojo_types::UntypedHandle;

use test_mojom::{AssociatedSender, HandleService, MathService, TwoInts, TypemapService};

use crate::state_objects::*;

#[gtest(RustBindingsAPI, MessagePipeWatcherBasicTests)]
fn test_watcher_basic() {
    // Exercise the various watcher methods so we have some coverage
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    use bindings::message_pipe_watcher::{MessagePipeWatcher, ResponseSender};
    use system::message::{FullyReadableWithHandlesMessage, WritableMessage};
    use system::message_pipe::MessageEndpoint;

    let (sender, receiver) = MessageEndpoint::create_pipe().unwrap();

    let received_messages: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(Vec::new()));
    let received_messages_clone = Arc::clone(&received_messages);

    // When we receive a message at the receiver, store it in `received_messages`
    // and send a simple response.
    let receiver_msg_handler = move |raw_msg: FullyReadableWithHandlesMessage,
                                     sender: ResponseSender| {
        let msg_contents = String::from_utf8(raw_msg.read_bytes().unwrap().to_vec()).unwrap();
        received_messages_clone.lock().unwrap().push(msg_contents);
        sender.try_send_response(
            WritableMessage::new_with_bytes(b"Got it!").unwrap().finalize_for_sending(),
        );
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

    let sender = MessagePipeWatcher::new(sender, sender_msg_handler, None, true).unwrap();
    let receiver = MessagePipeWatcher::new(receiver, receiver_msg_handler, None, true).unwrap();

    // Send some messages through; this should trigger the handler twice
    sender
        .send_message(WritableMessage::new_with_bytes(b"Message 1").unwrap().finalize_for_sending())
        .unwrap();
    sender
        .send_message(WritableMessage::new_with_bytes(b"Message 2").unwrap().finalize_for_sending())
        .unwrap();
    // Send a message the other way and make sure it arrived
    receiver
        .send_message(
            WritableMessage::new_with_bytes(b"From Receiver").unwrap().finalize_for_sending(),
        )
        .unwrap();

    run_loop.run();

    drop(sender);

    // Make sure we got 2 (the system handler ensures we got 3 responses)
    expect_eq!(2, received_messages.lock().unwrap().len());
}

#[gtest(RustBindingsAPI, MessagePipeWatcherDisconnectTests)]
fn test_watcher_disconnect() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
        true,
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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
    let receiver = MessagePipeWatcher::new(
        receiver,
        |_, _| {},
        Some(Box::new(receiver_disconnect_handler)),
        true,
    )
    .unwrap();

    run_loop.run();

    expect_false!(receiver.is_connected());
    expect_true!(*ran_disconnect_handler.lock().unwrap());
}

#[gtest(RustBindingsAPI, RemoteReceiverWrapMathTest)]
fn test_remote_receiver_wrapping() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    let (pending_remote, pending_receiver) = PendingRemote::<dyn MathService>::new_pipe().unwrap();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    // Pass the receiver handle to C++ and bind it there
    let receiver_wrapper =
        system::scoped_handle_interop::ScopedMessagePipeHandleWrapper::from_message_endpoint(
            pending_receiver.into_endpoint(),
        );
    let _cpp_receiver = crate::cxx::ffi::CreatePlusSevenMathService(receiver_wrapper);

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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
    let remote_wrapper =
        system::scoped_handle_interop::ScopedMessagePipeHandleWrapper::from_message_endpoint(
            pending_remote.into_endpoint(),
        );
    crate::cxx::ffi::TestRemoteFromCpp(remote_wrapper);

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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

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

#[gtest(RustBindingsAPI, CppToRustHandoverTest)]
fn test_cpp_to_rust_handover() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    // Create a PlusSevenMathService and bind it, all in C++.
    let mut _service = cxx::UniquePtr::null();
    let mut remote_wrapper = cxx::UniquePtr::null();
    crate::cxx::ffi::CreatePlusSevenMathServiceAndRemote(&mut _service, &mut remote_wrapper);

    // Convert the C++ endpoint to the equivalent Rust version.
    // Since we use the scoped_handle_interop types, this doesn't require `unsafe`!
    let remote_endpoint =
        system::scoped_handle_interop::ScopedMessagePipeHandleWrapper::into_message_endpoint(
            remote_wrapper,
        )
        .unwrap();
    let pending_remote = PendingRemote::<dyn MathService>::new(remote_endpoint);

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    let mut remote = pending_remote.bind();

    // Simple test message
    remote.Add(5, 10, move |n| {
        expect_eq!(n, 22); // PlusSevenMathService adds 7 to all its operations.
        quit();
    });

    run_loop.run();
}

#[gtest(RustBindingsAPI, TypemappingTest)]
fn test_typemapping() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    let (pending_remote, pending_receiver) =
        PendingRemote::<dyn TypemapService>::new_pipe().unwrap();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    let mut remote = pending_remote.bind();

    let _receiver = pending_receiver.bind(TypemapServiceImpl {});

    remote.Echo(test_mojom::MyCustomStruct { value: 42 }, move |res| {
        expect_eq!(res.value, 42);
        quit();
    });

    run_loop.run();
}

/// This is a helper function for setting up tests which use associated
/// interfaces.
///
/// Initial setup: In order to ensure that associated endpoints work, we need
/// to send messages between them. In order to do that, we need to
/// 1. Set up a real message pipe
/// 2. Send some pending associated endpoints across it
/// 3. Send messages across the associated pairs.
///
/// In order to do (3), we store one end of each associated pair in the
/// `AssociatedSenderImpl` object. We keep a clone of the object around which
/// shares its data with the original, so we can access the contained
/// endpoints during testing.
///
/// This function returns a set of associated interfaces, with one end of each
/// pair contained in the returned `AssociatedSenderImpl` and the other ends are
/// provided individually, in the same order as the fields in the `Impl`. The
/// function also provides the original Remote and Receiver to keep the
/// underlying message pipe alive.
///
/// This function also exercises the serialization code for associated
/// endpoints, checking that it works both before and after the other endpoint
/// is bound to a sequence.
#[allow(clippy::type_complexity)] // Yes, yes it is.
fn init_associated_test() -> (
    Remote<dyn AssociatedSender>,
    Receiver<AssociatedSenderImpl>,
    AssociatedSenderImpl,
    PendingAssociatedReceiver<dyn MathService>,
    PendingAssociatedRemote<dyn MathService>,
    PendingAssociatedRemote<dyn MathService>,
    PendingAssociatedReceiver<dyn MathService>,
) {
    let (pending_remote, pending_receiver) =
        PendingRemote::<dyn AssociatedSender>::new_pipe().unwrap();
    // This state object holds the other end of associated endpoints that are sent
    // across the pipe. We'll clone it so we can get them out again afterwards.
    let assoc_impl = AssociatedSenderImpl::new();

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    let mut remote = pending_remote.bind();
    let receiver = pending_receiver.bind(assoc_impl.clone());

    let (assoc_p_rem_1, assoc_p_rec_1) = PendingAssociatedRemote::new_pair();
    let (assoc_p_rem_2, assoc_p_rec_2) = PendingAssociatedRemote::new_pair();
    let assoc_p_rem_3: Arc<OnceLock<PendingAssociatedRemote<dyn MathService>>> =
        Arc::new(OnceLock::new());
    let assoc_p_rec_4: Arc<OnceLock<PendingAssociatedReceiver<dyn MathService>>> =
        Arc::new(OnceLock::new());

    let assoc_p_rem_3_clone = assoc_p_rem_3.clone();
    let assoc_p_rec_4_clone = assoc_p_rec_4.clone();

    remote.SendRemote(assoc_p_rem_1);
    remote.SendReceiver(assoc_p_rec_2);
    remote.RequestRemote(move |p_rem| {
        assoc_p_rem_3_clone.set(p_rem).unwrap();
    });
    remote.RequestReceiver(move |p_rec| {
        assoc_p_rec_4_clone.set(p_rec).unwrap();
        quit();
    });

    run_loop.run();

    return (
        remote,
        receiver,
        assoc_impl,
        assoc_p_rec_1,
        assoc_p_rem_2,
        Arc::into_inner(assoc_p_rem_3).unwrap().into_inner().unwrap(),
        Arc::into_inner(assoc_p_rec_4).unwrap().into_inner().unwrap(),
    );
}

/// Helper for creating a single-argument closure that increments the count in
/// $arc, checks that it has an expected value, and also checks that the
/// argument to closure itself has the expected value.
///
/// The final `quit` argument is optional. If present, `$quit()` will be run at
/// the end of the closure.
macro_rules! math_response_closure {
    ($arc:ident, $expected_count:expr, $expected_arg:expr $(, $quit:ident)?) => {{
        let arc_clone = Arc::clone(&$arc);
        move |n| {
            expect_eq!(n, $expected_arg);
            let mut count = arc_clone.lock().unwrap();
            expect_eq!(*count, $expected_count);
            *count += 1;
            $($quit())?
        }
    }};
}

#[gtest(RustBindingsAPI, TestAssociatedEndpoints)]
fn test_associated_endpoints() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    let (_rem, _rec, assoc_impl, _assoc_p_rec_1, assoc_p_rem_2, assoc_p_rem_3, _assoc_p_rec_4) =
        init_associated_test();

    // Corresponds to the `PendingAssociatedReceiver<dyn MathService>` in
    // `assoc_impl`
    let mut wrapping_remote = assoc_p_rem_2.bind();
    // Corresponds to the `AssociatedReceiver<SaturatingMathService>` in
    // `assoc_impl`
    let mut saturating_remote = assoc_p_rem_3.bind();
    let _wrapping_receiver =
        assoc_impl.send_receiver.lock().unwrap().take().unwrap().bind(WrappingMathService {});

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    let responses_received = Arc::new(Mutex::new(0));

    // Send some messages!
    wrapping_remote.Add(1, 2, math_response_closure!(responses_received, 0, 3));
    saturating_remote.Add(1, 2, math_response_closure!(responses_received, 1, 3));

    wrapping_remote.Add(1, u32::MAX, math_response_closure!(responses_received, 2, 0));
    saturating_remote.Add(1, u32::MAX, math_response_closure!(responses_received, 3, u32::MAX));

    wrapping_remote
        .AddTwoInts(TwoInts { a: 7, b: 12 }, math_response_closure!(responses_received, 4, 19));
    saturating_remote.AddTwoInts(
        TwoInts { a: 7, b: 12 },
        math_response_closure!(responses_received, 5, 19, quit),
    );

    run_loop.run();
}

// Test that messages for an unbound endpoint aren't delivered until it's bound,
// and that they block future messages.
#[gtest(RustBindingsAPI, TestAssociatedLateBinding)]
fn test_associated_late_binding() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    let (_rem, _rec, assoc_impl, _assoc_p_rec_1, assoc_p_rem_2, assoc_p_rem_3, _assoc_p_rec_4) =
        init_associated_test();

    // Corresponds to the `PendingAssociatedReceiver<dyn MathService>` in
    // `assoc_impl`
    let mut wrapping_remote = assoc_p_rem_2.bind();
    // Corresponds to the `AssociatedReceiver<SaturatingMathService>` in
    // `assoc_impl`
    let mut saturating_remote = assoc_p_rem_3.bind();

    let responses_received = Arc::new(Mutex::new(0));

    let run_loop = RunLoop::new();
    let quit = run_loop.get_quit_closure();

    // This should be delivered and run immediately
    saturating_remote.Add(1, u32::MAX, math_response_closure!(responses_received, 0, u32::MAX));

    // We haven't bound the wrapping receiver yet, so this should be queued
    // until we do
    wrapping_remote.Add(1, u32::MAX, math_response_closure!(responses_received, 1, 0));

    // This should be queued until the previous message is processed.
    saturating_remote.Add(1, 9, math_response_closure!(responses_received, 2, 10, quit));

    // Run until there's nothing left to do; this should run just the first task
    let run_loop_idle = RunLoop::new();
    run_loop_idle.run_until_idle();
    expect_eq!(*responses_received.lock().unwrap(), 1);

    // Now bind the receiver and try again
    let _receiver =
        assoc_impl.send_receiver.lock().unwrap().take().unwrap().bind(WrappingMathService {});

    run_loop.run();

    expect_eq!(*responses_received.lock().unwrap(), 3);
}

#[gtest(RustBindingsAPI, TestCppAssociatedSender)]
fn test_cpp_associated_sender() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    {
        let (pending_remote, pending_receiver) =
            PendingRemote::<dyn AssociatedSender>::new_pipe().unwrap();

        let receiver_wrapper =
            system::scoped_handle_interop::ScopedMessagePipeHandleWrapper::from_message_endpoint(
                pending_receiver.into_endpoint(),
            );
        crate::cxx::ffi::CreateCppAssociatedSender(receiver_wrapper);

        let mut remote = pending_remote.bind();

        let run_loop = RunLoop::new();
        let quit = run_loop.get_quit_closure();

        let count = Arc::new(Mutex::new(0));

        // Vectors to keep endpoints alive
        let active_remotes = Arc::new(Mutex::new(Vec::new()));
        let active_receivers = Arc::new(Mutex::new(Vec::new()));

        // 1. Send Remote to C++
        let (math_rem, math_rec) = PendingAssociatedRemote::<dyn MathService>::new_pair();
        let _math_receiver = math_rec.bind(crate::state_objects::NotifyingMathService {
            f: math_response_closure!(count, 0, 3),
        });
        remote.SendRemote(math_rem);

        // 2. Send Receiver to C++
        let (math_rem2, math_rec2) = PendingAssociatedRemote::<dyn MathService>::new_pair();
        let mut math_remote2 = math_rem2.bind();
        remote.SendReceiver(math_rec2);

        // Recall that C++ PlusSevenMathService adds 7 to the result
        math_remote2.Add(10, 20, math_response_closure!(count, 1, 37));

        // 3. Request Remote from C++
        let response_handler = math_response_closure!(count, 2, 17);
        let active_remotes_clone = active_remotes.clone();
        remote.RequestRemote(move |math_rem| {
            let mut math_remote = math_rem.bind();
            math_remote.Add(5, 5, response_handler);
            active_remotes_clone.lock().unwrap().push(math_remote);
        });

        // 4. Request Receiver from C++
        let f = math_response_closure!(count, 2, 50, quit);
        let active_receivers_clone = active_receivers.clone();
        remote.RequestReceiver(move |math_rec| {
            let receiver = math_rec.bind(crate::state_objects::NotifyingMathService { f });
            active_receivers_clone.lock().unwrap().push(receiver);
        });

        run_loop.run();
    }
    // We need to make sure the disconnect handlers are actually run to avoid
    // complaints about memory leaks.
    let run_loop_idle = RunLoop::new();
    run_loop_idle.run_until_idle();
}

#[gtest(RustBindingsAPI, TestAssociatedDisconnect)]
fn test_associated_disconnect() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));

    let (
        _remote,
        _receiver,
        assoc_impl,
        assoc_p_rec_1,
        assoc_p_rem_2,
        _assoc_p_rem_3,
        _assoc_p_rec_4,
    ) = init_associated_test();

    // 1. Test Remote-to-Receiver disconnection
    {
        let run_loop = RunLoop::new();
        let quit = run_loop.get_quit_closure();

        let _math_receiver_1 = assoc_p_rec_1.bind_with_options(
            crate::state_objects::SaturatingMathService {},
            None,
            Some(Box::new(quit)),
        );

        // Drop the remote end, which is stored in assoc_impl.send_remote
        drop(assoc_impl.send_remote.lock().unwrap().take());

        run_loop.run();
    }

    // 2. Test Receiver-to-Remote disconnection
    {
        let run_loop = RunLoop::new();
        let quit = run_loop.get_quit_closure();

        let _math_remote_2 = assoc_p_rem_2.bind_with_options(None, Some(Box::new(quit)));

        // Drop the receiver end, which is stored in assoc_impl.send_receiver
        drop(assoc_impl.send_receiver.lock().unwrap().take());

        run_loop.run();
    }
}

#[gtest(RustBindingsAPI, TestBadControlMessage)]
fn test_bad_control_message() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();

    let bad_message_flag = Arc::new(Mutex::new(None::<String>));
    let bad_message_flag_clone = bad_message_flag.clone();
    test_util::set_default_process_error_handler(move |msg: &str| {
        *bad_message_flag_clone.lock().unwrap() = Some(msg.to_string());
    });

    let (handle0, handle1) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();

    let pending_receiver = PendingReceiver::<dyn MathService>::new(handle0);
    let _receiver = pending_receiver.bind(crate::state_objects::WrappingMathService {});

    // Send a bad control message on handle1 (wrong message name)
    let header = MessageHeader::new(
        u32::MAX, // CONTROL_INTERFACE_ID
        0,        // Wrong message name (should be 0xFFFFFFFE)
        MessageHeaderFlags::default(),
        0,
        0,
    );
    let msg = MojomMessage { header, payload: vec![], handles: vec![], raw_message_handle: None };
    let (serialized, handles) = msg.into_data();
    let raw_msg = system::message::WritableMessage::new_with_data(&serialized, handles)
        .unwrap()
        .finalize_for_sending();
    handle1.write(raw_msg).unwrap();

    // Run the loop to process the message
    let run_loop = RunLoop::new();
    run_loop.run_until_idle();

    // Check that bad message was reported
    let reported = bad_message_flag.lock().unwrap().take();
    expect_true!(reported.is_some());
    expect_eq!(reported.unwrap(), "Control message has incorrect message ID");
}

// These types and functions provide us a way to set a disconnect handler for
// C++ remotes/receivers that will end up calling back into rust so we can
// easily track it. It stores a function that takes an integer so we can
// distinguish different remotes/receivers calling their DC handlers.
type DisconnectInfoCallback = Box<dyn Fn(i32) + Send>;
static CPP_DISCONNECT_CALLBACK: Mutex<Option<DisconnectInfoCallback>> = Mutex::new(None);

#[unsafe(no_mangle)]
pub extern "C" fn rust_on_cpp_associated_disconnect(handler_type: i32) {
    if let Some(cb) = CPP_DISCONNECT_CALLBACK.lock().unwrap().as_ref() {
        cb(handler_type);
    }
}

#[gtest(RustBindingsAPI, TestAssociatedDisconnectCpp)]
fn test_associated_disconnect_cpp() {
    let _task_env = task_environment::ffi::CreateTaskEnvironment();
    test_util::set_default_process_error_handler(|msg: &str| panic!("Got a bad message: {}", msg));
    {
        // Set up the primary pipe
        let (pending_remote, pending_receiver) =
            PendingRemote::<dyn AssociatedSender>::new_pipe().unwrap();

        // Pass the primary receiver to C++
        crate::cxx::ffi::CreateCppAssociatedSender(
            system::scoped_handle_interop::ScopedMessagePipeHandleWrapper::from_message_endpoint(
                pending_receiver.into_endpoint(),
            ),
        );

        let mut remote = pending_remote.bind();

        // Test Rust to C++ disconnection
        // We create a pair, send the receiver to C++, and drop the remote in Rust.
        {
            let (assoc_p_rem, assoc_p_rec) = PendingAssociatedRemote::new_pair();

            remote.SendReceiver(assoc_p_rec);

            let run_loop = RunLoop::new();
            let quit = run_loop.get_quit_closure();

            // Register the Rust callback for C++ disconnect
            *CPP_DISCONNECT_CALLBACK.lock().unwrap() = Some(Box::new(move |handler_type| {
                expect_eq!(handler_type, 2); // C++ PlusSevenMathService (type 2) disconnected
                quit();
            }));

            // Drop the remote end on the Rust side
            drop(assoc_p_rem);

            // This loop won't terminate until the C++ DC callback executes
            run_loop.run();
        }

        // Reset the C++ callback state
        *CPP_DISCONNECT_CALLBACK.lock().unwrap() = None;

        // Same as above, but in the other direction
        {
            let (assoc_p_rem, assoc_p_rec) = PendingAssociatedRemote::new_pair();

            remote.SendRemote(assoc_p_rem);

            let run_loop = RunLoop::new();
            let quit = run_loop.get_quit_closure();

            let _math_receiver = assoc_p_rec.bind_with_options(
                crate::state_objects::SaturatingMathService {},
                None,
                Some(Box::new(quit)),
            );

            // Send a message that will drop the remote on the other side.
            remote.ClearActiveEndpoints();

            run_loop.run();
        }
    }

    // Make sure to clean up the self-owned receiver on the other side
    RunLoop::new().run_until_idle();
}
