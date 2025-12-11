// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Condvar, LazyLock, Mutex};

chromium::import! {
    pub "//mojo/public/rust:mojo_rust_system_api" as system;
    pub "//mojo/public/rust/test_support:test_util" as test_util;
}

// Mimics the tests in //mojo/public/c/system/tests/core_api_unittests.cc,
// but testing the Rust API's guarantees for that functionality rather
// than the C++ API's.
#[gtest(RustSystemAPITestSuite, BasicMessageWriteAndSendTest)]
fn test_basic_message_write_and_send() {
    // Tests a basic creation of a pipe and tries to send a message over it.
    // FOR_RELEASE: Do we need to invoke this per-test or can it be invoked
    // once?
    test_util::init_mojo_if_needed();

    // In the C API, creation of a message pipe is done by first instantiating
    // two invalid MojoHandles, passing those to MojoCreateMessagePipe,
    // and checking the result.
    //
    // In the Rust API you should never directly touch an invalid MojoHandle.
    // The MojoHandles are created under the hood here.
    let (endpoint_a, endpoint_b) = system::mojo_types::create_message_pipe().unwrap();

    // In the C API this looks like:
    //   MojoMessageHandle message;
    //   MojoResult result = MojoCreateMessage(nullptr, &message);
    //   result = MojoWriteMessage(endpoint, message, nullptr);
    // We simplify all this logic into the `write` function on the endpoint.
    let hello = b"hello";
    let write_result = endpoint_b.write(hello, Vec::new());
    expect_eq!(write_result, system::mojo_types::MojoResult::Okay);

    // Attempt to read the result.
    let (hello_data, _) = endpoint_a.read().expect("failed to read from endpoint_a");
    expect_eq!(String::from_utf8(hello_data), Ok("hello".to_string()));

    // Additional C++ unit tests include:
    // * core
    // * data_pipe_drainer
    // * data_pipe_producer
    // * data_pipe_unittests
    // * file_data_source
    // * file_stream_data_source
    // * handle_signal_tracker
    // * handle_signals_state
    // * invitation (I think we're ignoring these for now?)
    // * scope_to_messagE_pipe
    // * simple_watcher
    // * string_data_source
    // * wait_set
    // * wait.

    // FOR_RELEASE: Implement all the above.
    assert_eq!(0, 0);
}

#[gtest(RustSystemAPITestSuite, DataPipeWriteAndSendTest)]
fn test_data_pipe_write_and_send() {
    test_util::init_mojo_if_needed();

    let (consumer, mut producer) = system::data_pipe::create(5).unwrap();

    let hello = b"hello";
    let bytes_written =
        producer.write_with_flags(hello, system::data_pipe::WriteFlags::empty()).unwrap();
    expect_eq!(bytes_written, hello.len());

    let mut read_buffer = [0u8; 5];
    let bytes_read =
        consumer.read_with_flags(&mut read_buffer, system::data_pipe::ReadFlags::empty()).unwrap();
    expect_eq!(&read_buffer[..bytes_read], hello);

    // TODO: implement and test two-phase read-write.

    assert_eq!(0, 0);
}

#[gtest(RustSystemAPITestSuite, TrapSignalOnReadableTest)]
fn test_raw_trap_signal_on_readable() {
    test_util::init_mojo_if_needed();

    // We need a few global values to keep track of our test trap events.
    static TEST_TRAP_EVENT_LIST: LazyLock<Mutex<Vec<system::raw_trap::RawTrapEvent>>> =
        LazyLock::new(|| Mutex::new(Vec::new()));
    static TEST_TRAP_EVENT_COND: LazyLock<Condvar> = LazyLock::new(|| Condvar::new());

    // Helper handler for testing.
    extern "C" fn test_trap_event_handler(event: &system::raw_trap::RawTrapEvent) {
        // If locking fails, it means another thread panicked. In this case we can
        // simply do nothing. Note that we cannot panic here since this is called
        // from C code.
        if let Ok(mut list) = TEST_TRAP_EVENT_LIST.lock() {
            list.push(*event);
            TEST_TRAP_EVENT_COND.notify_all();
        }
    }

    // Helper function for testing.
    fn wait_for_trap_events(
        guard: std::sync::MutexGuard<'static, Vec<system::raw_trap::RawTrapEvent>>,
        expected_len: usize,
    ) -> std::sync::MutexGuard<'static, Vec<system::raw_trap::RawTrapEvent>> {
        TEST_TRAP_EVENT_COND.wait_while(guard, |l| l.len() < expected_len).unwrap()
    }

    // Make a new trap.
    let trap = system::raw_trap::RawTrap::new(test_trap_event_handler).unwrap();

    // Make a data pipe pair and add a trigger to both ends of the pipe.
    let (consumer, mut producer) = system::data_pipe::create(0).unwrap();
    expect_eq!(
        system::mojo_types::MojoResult::Okay,
        trap.add_trigger(
            &consumer,
            system::mojo_types::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::SignalsSatisfied,
            1,
        )
    );
    expect_eq!(
        system::mojo_types::MojoResult::Okay,
        trap.add_trigger(
            &producer,
            system::mojo_types::HandleSignals::PEER_CLOSED,
            system::raw_trap::TriggerCondition::SignalsSatisfied,
            2,
        )
    );

    let mut blocking_events_buf = [std::mem::MaybeUninit::uninit(); 16];
    // The trap should arm with no blocking events since nothing should be
    // triggered yet.
    match trap.arm(Some(&mut blocking_events_buf)) {
        system::raw_trap::ArmResult::Armed => (),
        system::raw_trap::ArmResult::Blocked(events) => {
            expect_true!(false, "unexpected blocking events {:?}", events)
        }
        system::raw_trap::ArmResult::Failed(e) => {
            expect_true!(false, "unexpected mojo error {:?}", e)
        }
    }

    expect_eq!(
        producer.write_with_flags(&[128u8], system::data_pipe::WriteFlags::empty()).unwrap(),
        1
    );
    {
        let list = wait_for_trap_events(TEST_TRAP_EVENT_LIST.lock().unwrap(), 1);
        expect_eq!(list.len(), 1);
        let event = list[0];
        expect_eq!(event.trigger_context(), 1);
        expect_eq!(event.result(), system::mojo_types::MojoResult::Okay);
        expect_true!(
            event.signals_state().satisfiable().is_readable(),
            "{:?}",
            event.signals_state()
        );
        expect_true!(
            event.signals_state().satisfied().is_readable(),
            "{:?}",
            event.signals_state()
        );
    }

    // FOR_RELEASE: Remaining trap tests. Check that re-arming doesn't work for
    // this scenario. Add higher-level Trap function to be used by most clients.
}

#[gtest(RustSystemAPITestSuite, AttemptToAddOrRemoveTriggerWithSameContextTwice)]
fn test_raw_trap_c_layer_attempts_to_remove_context_twice() {
    test_util::init_mojo_if_needed();
    extern "C" fn test_trap_event_handler(_event: &system::raw_trap::RawTrapEvent) {}
    let trap = system::raw_trap::RawTrap::new(test_trap_event_handler).unwrap();

    // Create a data pipe and add a trigger with a dummy CONTEXT.
    let (consumer, _) = system::data_pipe::create(0).unwrap();
    const CONTEXT: usize = 123;
    expect_eq!(
        system::mojo_types::MojoResult::Okay,
        trap.add_trigger(
            &consumer,
            system::mojo_types::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::SignalsSatisfied,
            CONTEXT,
        )
    );
    expect_eq!(
        system::mojo_types::MojoResult::AlreadyExists,
        trap.add_trigger(
            &consumer,
            system::mojo_types::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::SignalsSatisfied,
            CONTEXT,
        )
    );

    expect_eq!(system::mojo_types::MojoResult::Okay, trap.remove_trigger(CONTEXT));

    expect_eq!(system::mojo_types::MojoResult::NotFound, trap.remove_trigger(CONTEXT));
}

#[gtest(RustSystemAPITestSuite, MakeRegularTrap)]
fn test_make_regular_trap() {
    test_util::init_mojo_if_needed();

    let _trap = system::safe_trap::Trap::new().unwrap();
}
