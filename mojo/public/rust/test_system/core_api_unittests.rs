// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Arc, Condvar, LazyLock, Mutex};

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

#[gtest(RustSystemAPITestSuite, MessagePipes_RawTrapSignalOnReadableTest)]
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
    fn wait_for_asynchronously_delivered_trap_events(
        test_trap_event_list: &Mutex<Vec<system::raw_trap::RawTrapEvent>>,
        target_len: usize,
    ) -> Vec<system::raw_trap::RawTrapEvent> {
        let start = std::time::Instant::now();
        // Because Mojo message pipes are asynchronous, we need a sleep/yield loop to
        // check for trap events.
        // FOR RELEASE: Re-implement on top of Rust bindings for `base::Run::Loop`
        // when/if available in the future.
        loop {
            {
                let list = test_trap_event_list.lock().unwrap();
                if list.len() >= target_len {
                    return list.clone();
                }
            }

            if start.elapsed().as_secs() > 5 {
                panic!("Timed out waiting for trap events");
            }
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
    }

    // Helper function for testing.
    fn clear_trap_events(target_len: usize) {
        let mut list = TEST_TRAP_EVENT_LIST.lock().unwrap();
        expect_eq!(list.len(), target_len, "unexpected events {:?}", *list);
        list.clear();
    }

    // Make a new trap.
    let trap = system::raw_trap::RawTrap::new(test_trap_event_handler).unwrap();

    // Make a message pipe pair and add a trigger to both ends of the pipe.
    let (endpoint_a, endpoint_b) = system::mojo_types::create_message_pipe().unwrap();
    expect_eq!(
        system::mojo_types::MojoResult::Okay,
        trap.add_trigger(
            &endpoint_a,
            system::mojo_types::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::SignalsSatisfied,
            1,
        )
    );

    expect_eq!(
        system::mojo_types::MojoResult::Okay,
        trap.add_trigger(
            &endpoint_b,
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

    let hello = b"hello";
    expect_eq!(endpoint_b.write(hello, Vec::new()), system::mojo_types::MojoResult::Okay);
    {
        let list = wait_for_asynchronously_delivered_trap_events(&TEST_TRAP_EVENT_LIST, 1);
        expect_eq!(list.len(), 1);

        let event = list[0];
        expect_eq!(event.trigger_context(), 1);
        expect_eq!(event.result(), system::mojo_types::MojoResult::Okay);
        expect_true!(
            event.signals_state().satisfiable().is_readable(),
            "{:?}",
            event.signals_state()
        );
    }

    // Once the event has fired, `trap` is disarmed.

    // Re-arming should block and return the same event from before.
    match trap.arm(Some(&mut blocking_events_buf)) {
        system::raw_trap::ArmResult::Armed => {
            expect_true!(false, "trap incorrectly remained armed after event arrived")
        }
        system::raw_trap::ArmResult::Blocked(events) => {
            let event = events.get(0).unwrap();
            expect_eq!(event.trigger_context(), 1);
            expect_eq!(event.result(), system::mojo_types::MojoResult::Okay);
        }
        system::raw_trap::ArmResult::Failed(e) => {
            expect_true!(false, "unexpected Mojo error {:?}", e)
        }
    }

    clear_trap_events(1);

    // Read the data so we don't receive the same event again.
    let (_, _) = endpoint_a.read().expect("failed to read from endpoint_a");

    match trap.arm(Some(&mut blocking_events_buf)) {
        system::raw_trap::ArmResult::Armed => (),
        system::raw_trap::ArmResult::Blocked(events) => {
            expect_true!(false, "unexpected blocking events {:?}", events)
        }
        system::raw_trap::ArmResult::Failed(e) => {
            expect_true!(false, "unexpected Mojo error {:?}", e)
        }
    }

    // Drop endpoint_b, which should make endpoint_a unreadable.
    drop(endpoint_b);

    // Now we expect two events.
    // One indicates that endpoint_a is no longer readable (`FailedPrecondition`).
    // The other indicates that endpoint_b was closed (`Cancelled`).
    let list = wait_for_asynchronously_delivered_trap_events(&TEST_TRAP_EVENT_LIST, 1);
    expect_eq!(list.len(), 2);
    let (event1, event2) = (list[0], list[1]);
    // Sort the events since the ordering isn't deterministic.
    let (endpoint_a_event, endpoint_b_event) =
        if event1.trigger_context() == 1 { (event1, event2) } else { (event2, event1) };

    // `endpoint_a`` is no longer readable.
    expect_eq!(endpoint_a_event.trigger_context(), 1);
    expect_eq!(endpoint_a_event.result(), system::mojo_types::MojoResult::FailedPrecondition);
    expect_true!(!endpoint_a_event.signals_state().satisfiable().is_readable());

    // `endpoint_b`` was cancelled (dropped).
    expect_eq!(endpoint_b_event.trigger_context(), 2);
    expect_eq!(endpoint_b_event.result(), system::mojo_types::MojoResult::Cancelled);

    drop(trap);

    // There should be three events: the two already described above, plus a
    // `Cancelled` event for removing endpoint_a from the trap (which happens
    // automatically upon dropping `trap`.)
    clear_trap_events(3);
}

#[gtest(RustSystemAPITestSuite, MessagePipes_TrapSignalOnReadableTest)]
fn test_raw_trap_signal_on_readable() {
    test_util::init_mojo_if_needed();

    let (endpoint_a, endpoint_b) = system::mojo_types::create_message_pipe().unwrap();

    // 1. Create the safe Trap.
    let trap = system::safe_trap::Trap::new().expect("Failed to create safe Trap");

    // 2. We use a Mutex/Condvar to wait for the event in the main thread.
    let hit_count = Arc::new(Mutex::new(0));
    let condvar = Arc::new(Condvar::new());

    let hit_count_clone = Arc::clone(&hit_count);
    let condvar_clone = Arc::clone(&condvar);

    let _trigger_id = trap
        .add_trigger(
            &endpoint_a,
            system::mojo_types::HandleSignals::READABLE,
            system::safe_trap::TriggerCondition::SignalsSatisfied,
            move |event| {
                if event.result().is_ok() {
                    let mut count = hit_count_clone.lock().unwrap();
                    *count += 1;
                    condvar_clone.notify_all();
                }
            },
        )
        .expect("Failed to add trigger");

    trap.arm(system::safe_trap::ArmingPolicyForBlockingEvents::RearmUntilNoBlockingEvents)
        .expect("Failed to arm trap");

    let write_result = endpoint_b.write(b"hello", Vec::new());
    expect_eq!(write_result, system::mojo_types::MojoResult::Okay);

    let count = hit_count.lock().unwrap();
    let final_count =
        condvar.wait_timeout_while(count, std::time::Duration::from_secs(2), |c| *c == 0).unwrap();

    expect_eq!(*final_count.0, 1, "Should have fired once");
}

#[gtest(RustSystemAPITestSuite, CloseSafeTrapWithActiveTrigger)]
fn test_clos_safe_trap_with_active_trigger() {
    // Trap must do some lifecycle management/teardown of the pointers it encloses
    // when it is `drop`'d.
    //
    // Additionally we expect remove_trigger to be called on each active trigger,
    // and the associated callback to return TrapError::Cancelled.
    test_util::init_mojo_if_needed();
    let trap = system::safe_trap::Trap::new().expect("Failed to create safe Trap");
    let (ep_a, _ep_b) = system::mojo_types::create_message_pipe().unwrap();

    trap.add_trigger(
        &ep_a,
        system::mojo_types::HandleSignals::READABLE,
        system::safe_trap::TriggerCondition::SignalsSatisfied,
        move |event| {
            println!(
                "Trigger fired with result {:?} and signals {:?}",
                event.result(),
                event.signals_state()
            );
            expect_eq!(event.result(), Err(system::safe_trap::TrapError::Cancelled));
        },
    )
    .expect(&format!("Failed to add trigger"));
    drop(trap);
    // `drop` completed without any errors wrt pointer management and such.
    assert!(true);
}

#[gtest(RustSystemAPITestSuite, SafeTrapMultipleBlockingEvents)]
fn test_safe_trap_multiple_blocking_events() {
    test_util::init_mojo_if_needed();
    let trap = system::safe_trap::Trap::new().expect("Failed to create safe Trap");
    const NUM_TRIGGERS: usize = 20; // More than MAX_BLOCKING_EVENTS

    let callback_count = Arc::new(Mutex::new(0));
    let mut endpoints_a = Vec::with_capacity(NUM_TRIGGERS);
    let mut endpoints_b = Vec::with_capacity(NUM_TRIGGERS);

    // 1. Create NUM_TRIGGERS message pipe pairs. For each, add a trigger.
    for i in 0..NUM_TRIGGERS {
        let (ep_a, ep_b) = system::mojo_types::create_message_pipe().unwrap();
        let ep_a_arc = Arc::new(ep_a);

        let callback_count_clone = Arc::clone(&callback_count);
        let ep_a_clone = Arc::clone(&ep_a_arc);
        trap.add_trigger(
            &*ep_a_arc,
            system::mojo_types::HandleSignals::READABLE,
            system::safe_trap::TriggerCondition::SignalsSatisfied,
            move |event| {
                println!(
                    "Trigger {} fired with result {:?} and signals {:?}",
                    i,
                    event.result(),
                    event.signals_state()
                );
                match event.result() {
                    Ok(()) => {
                        // Standard behavior we expect for this test.
                        // We are setting 20 triggers so we will expect it to trigger with Ok()
                        // 20 times.
                        let mut count = callback_count_clone.lock().unwrap();
                        *count += 1;
                        ep_a_clone.read().expect("Failed to read from ep_a in callback");
                    }
                    Err(system::safe_trap::TrapError::Cancelled) => {
                        // Do not increase the callback count, as this is not a callback
                        // we're interested in measuring, but don't panic either.
                        // We expect this at the conclusion of our test, when Trap is dropped.
                        println!("Trigger {} was cancelled. This is expected during Trap drop.", i);
                    }
                    Err(system::safe_trap::TrapError::FailedPrecondition) => {
                        // Since we manually Drop our Trap at the end of the test,
                        // it should not be possible for the pipes we're monitoring to
                        // close or go out of scope before the Trap does, which is
                        // the only way this branch could be triggered.
                        // This indicates an error for this specific test (though not
                        // in general! in real code we would gracefully handle a pipe
                        // unexpectedly becoming unreadable)
                        panic!("Trigger {} failed with FailedPrecondition.", i);
                    }
                }
            },
        )
        .expect(&format!("Failed to add trigger {}", i));

        // 2. Trigger the READABLE signal on ep_a by writing to ep_b.
        // This creates a blocking event for each trigger.
        let write_result = ep_b.write(b"x", Vec::new());
        expect_eq!(write_result, system::mojo_types::MojoResult::Okay);
        endpoints_a.push(ep_a_arc); // Keep ep_a alive
        endpoints_b.push(ep_b); // Keep ep_a alive
    }

    // 3. Call trap.arm(). This should now handle all 20 blocking events.
    trap.arm(system::safe_trap::ArmingPolicyForBlockingEvents::RearmUntilNoBlockingEvents)
        .expect("Trap failed to arm after processing multiple blocking events");

    // 4. Verify that all NUM_TRIGGERS callbacks were executed.
    let final_count = *callback_count.lock().unwrap();
    expect_eq!(final_count, NUM_TRIGGERS, "Not all blocking events were processed");

    // 5. Verify the trap is now genuinely armed by calling arm again.
    // Since all blocking events have been handled, this call should immediately
    // return Armed.
    trap.arm(system::safe_trap::ArmingPolicyForBlockingEvents::RearmUntilNoBlockingEvents)
        .expect("Trap failed to re-arm after clearing all blocking events");
    // Manually drop our trap to ensure teardown behavior is as expected
    // (that is, Cancelled returned harmelssly for the various triggers
    // upon removal.)
    drop(trap);
}

// We test the majority of our trap functionality via MessagePipes.
// These DataPipe tests are thus somewhat redundant, but fine to keep for now.
#[gtest(RustSystemAPITestSuite, DataPipes_RawTrapSignalOnReadableTest)]
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
    fn wait_for_synchronously_delivered_trap_events(
        test_trap_event_list: &Mutex<Vec<system::raw_trap::RawTrapEvent>>,
        target_len: usize,
    ) -> Vec<system::raw_trap::RawTrapEvent> {
        let list_guard = test_trap_event_list.lock().unwrap();
        // Because Mojo data pipes are synchronous, we can simply wait behind a condvar.
        let guard =
            TEST_TRAP_EVENT_COND.wait_while(list_guard, |list| list.len() < target_len).unwrap();
        guard.clone()
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
        let list = wait_for_synchronously_delivered_trap_events(&TEST_TRAP_EVENT_LIST, 1);
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
