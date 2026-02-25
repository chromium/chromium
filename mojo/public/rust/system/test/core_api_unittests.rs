// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Arc, Condvar, LazyLock, Mutex};

chromium::import! {
    "//mojo/public/rust/system";
    "//mojo/public/rust/system/test_util";
}

// Mimics the tests in //mojo/public/c/system/tests/core_api_unittests.cc,
// but testing the Rust API's guarantees for that functionality rather
// than the C++ API's.
#[gtest(RustSystemAPITestSuite, BasicMessageWriteAndSendTest)]
fn test_basic_message_write_and_send() {
    // Tests a basic creation of a pipe and tries to send a message over it.
    // FOR_RELEASE: Do we need to invoke this per-test or can it be invoked
    // once?
    //
    // In the C API, creation of a message pipe is done by first instantiating
    // two invalid MojoHandles, passing those to MojoCreateMessagePipe,
    // and checking the result.
    //
    // In the Rust API you should never directly touch an invalid MojoHandle.
    // The MojoHandles are created under the hood here.
    let (endpoint_a, endpoint_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();
    let (dummy_handle, _) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();

    let hello = system::message::RawMojoMessage::new_with_data(b"hello", vec![dummy_handle.into()])
        .unwrap();

    let write_result = endpoint_b.write(hello);
    expect_true!(write_result.is_ok());

    // Attempt to read the result.
    let hello_msg = endpoint_a.read().expect("failed to read from endpoint_a");
    expect_eq!(
        String::from_utf8(hello_msg.read_bytes().unwrap().to_vec()),
        Ok("hello".to_string())
    );
    // Call the other read function just so we have some coverage of it
    // The function may only be called once per message, so the second call should
    // fail.
    let (_, _) = hello_msg.read_data().unwrap();
    expect_true!(hello_msg.read_data().is_err());
    // Calling read_bytes is independent of read_data and can be done many times.
    let _ = hello_msg.read_bytes().unwrap();
    let _ = hello_msg.read_bytes().unwrap();

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
    // * scope_to_message_pipe
    // * simple_watcher
    // * string_data_source
    // * wait_set
    // * wait.

    // FOR_RELEASE: Implement all the above.
}

#[gtest(RustSystemAPITestSuite, DataPipeWriteAndSendTest)]
fn test_data_pipe_write_and_send() {
    let (producer, consumer) = system::data_pipe::create(5).unwrap();

    let hello = b"hello";
    let bytes_written =
        producer.write_with_flags(hello, system::data_pipe::WriteFlags::empty()).unwrap();
    expect_eq!(bytes_written, hello.len());

    let mut read_buffer = [std::mem::MaybeUninit::uninit(); 5];
    let bytes_read =
        consumer.read_with_flags(&mut read_buffer, system::data_pipe::ReadFlags::empty()).unwrap();

    expect_eq!(bytes_read, 5);

    // SAFETY: `read_with_flags` promises that `bytes_read` bytes are now
    // initialized
    let read_buffer: [u8; 5] = unsafe { std::mem::transmute(read_buffer) };
    expect_eq!(&read_buffer, hello);

    // TODO: implement and test two-phase read-write.
}

#[gtest(RustSystemAPITestSuite, MessagePipes_RawTrapSignalOnReadableTest)]
fn test_raw_trap_signal_on_readable() {
    // We need a few global values to keep track of our test trap events.
    static TEST_TRAP_EVENT_LIST: LazyLock<Mutex<Vec<system::raw_trap::RawTrapEvent>>> =
        LazyLock::new(|| Mutex::new(Vec::new()));
    static TEST_TRAP_EVENT_COND: LazyLock<Condvar> = LazyLock::new(Condvar::new);

    // Helper handler for testing.
    extern "C" fn test_trap_event_handler(event: &system::raw_trap::RawTrapEvent) {
        // If locking fails, it means another thread panicked. In this case we can
        // simply do nothing. Note that we cannot panic here since this is called
        // from C code.
        if let Ok(mut list) = TEST_TRAP_EVENT_LIST.lock() {
            list.push(event.clone());
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
    let (endpoint_a, endpoint_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();
    expect_true!(trap
        .add_trigger(
            &endpoint_a,
            system::raw_trap::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
            1,
        )
        .is_ok());

    expect_true!(trap
        .add_trigger(
            &endpoint_b,
            system::raw_trap::HandleSignals::PEER_CLOSED,
            system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
            2,
        )
        .is_ok());

    let mut blocking_events_buf = [const { std::mem::MaybeUninit::uninit() }; 16];
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

    let hello = system::message::RawMojoMessage::new_with_bytes(b"hello").unwrap();
    expect_true!(endpoint_b.write(hello).is_ok());
    {
        let list = wait_for_asynchronously_delivered_trap_events(&TEST_TRAP_EVENT_LIST, 1);
        expect_eq!(list.len(), 1);

        let event = &list[0];
        expect_eq!(event.trigger_context(), 1);
        expect_true!(event.result().is_ok());
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
            let event = events.first().unwrap();
            expect_eq!(event.trigger_context(), 1);
            expect_true!(event.result().is_ok());
        }
        system::raw_trap::ArmResult::Failed(e) => {
            expect_true!(false, "unexpected Mojo error {:?}", e)
        }
    }

    clear_trap_events(1);

    // Read the data so we don't receive the same event again.
    let _ = endpoint_a.read().expect("failed to read from endpoint_a");

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
    let (event1, event2) = (&list[0], &list[1]);
    // Sort the events since the ordering isn't deterministic.
    let (endpoint_a_event, endpoint_b_event) =
        if event1.trigger_context() == 1 { (event1, event2) } else { (event2, event1) };

    // `endpoint_a`` is no longer readable.
    expect_eq!(endpoint_a_event.trigger_context(), 1);
    expect_eq!(endpoint_a_event.result(), Err(system::mojo_types::MojoError::FailedPrecondition));
    expect_true!(!endpoint_a_event.signals_state().satisfiable().is_readable());

    // `endpoint_b`` was cancelled (dropped).
    expect_eq!(endpoint_b_event.trigger_context(), 2);
    expect_eq!(endpoint_b_event.result(), Err(system::mojo_types::MojoError::Cancelled));

    drop(trap);

    // There should be three events: the two already described above, plus a
    // `Cancelled` event for removing endpoint_a from the trap (which happens
    // automatically upon dropping `trap`.)
    clear_trap_events(3);
}

#[gtest(RustSystemAPITestSuite, MessagePipes_TrapSignalOnReadableTest)]
fn test_raw_trap_signal_on_readable() {
    let (endpoint_a, endpoint_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();

    // 1. Create the safe Trap.
    let trap = system::trap::Trap::new(system::trap::RearmingPolicy::Manual)
        .expect("Failed to create safe Trap");

    // 2. We use a Mutex/Condvar to wait for the event in the main thread.
    let hit_count = Arc::new(Mutex::new(0));
    let condvar = Arc::new(Condvar::new());

    let hit_count_clone = Arc::clone(&hit_count);
    let condvar_clone = Arc::clone(&condvar);

    let _trigger_id = trap.add_trigger(
        &endpoint_a,
        system::raw_trap::HandleSignals::READABLE,
        system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
        move |event| {
            if event.result().is_ok() {
                let mut count = hit_count_clone.lock().unwrap();
                *count += 1;
                condvar_clone.notify_all();
            }
        },
    );

    trap.arm(system::trap::InitialArmingPolicy::RunTriggersOnBlockingEvents)
        .expect("Failed to arm trap");

    let hello = system::message::RawMojoMessage::new_with_bytes(b"hello").unwrap();
    let write_result = endpoint_b.write(hello);
    expect_true!(write_result.is_ok());
    {
        let count = hit_count.lock().unwrap();
        let final_count = condvar
            .wait_timeout_while(count, std::time::Duration::from_secs(2), |c| *c == 0)
            .unwrap();

        expect_eq!(*final_count.0, 1, "Should have fired once");
    }
}

#[gtest(RustSystemAPITestSuite, MessagePipes_TrapAutoRearmTest)]
fn test_trap_auto_rearm() {
    let (endpoint_a, endpoint_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();
    let endpoint_a = Arc::new(endpoint_a);
    let endpoint_a_weak = Arc::downgrade(&endpoint_a);

    // 1. Create the safe Trap.
    let trap = system::trap::Trap::new(system::trap::RearmingPolicy::Automatic)
        .expect("Failed to create safe Trap");

    // 2. We use a Mutex/Condvar to wait for the event in the main thread.
    let hit_count = Arc::new(Mutex::new(0));
    let condvar = Arc::new(Condvar::new());

    let hit_count_clone = Arc::clone(&hit_count);
    let condvar_clone = Arc::clone(&condvar);

    let _trigger_id = trap.add_trigger(
        &*endpoint_a,
        system::raw_trap::HandleSignals::READABLE,
        system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
        move |event| {
            if event.result().is_ok() {
                let mut count = hit_count_clone.lock().unwrap();
                *count += 1;
                condvar_clone.notify_all();
            }
            let _ = endpoint_a_weak.upgrade().unwrap().read();
        },
    );

    trap.arm(system::trap::InitialArmingPolicy::RunTriggersOnBlockingEvents)
        .expect("Failed to arm trap");

    let hello = system::message::RawMojoMessage::new_with_bytes(b"hello").unwrap();
    let write_result = endpoint_b.write(hello);
    expect_true!(write_result.is_ok());
    {
        let count = hit_count.lock().unwrap();
        let final_count = condvar
            .wait_timeout_while(count, std::time::Duration::from_secs(2), |c| *c == 0)
            .unwrap();

        expect_eq!(*final_count.0, 1, "Should have fired once");
    }

    let hello2 = system::message::RawMojoMessage::new_with_bytes(b"hello2").unwrap();
    let write_result = endpoint_b.write(hello2);
    expect_true!(write_result.is_ok());

    {
        let count = hit_count.lock().unwrap();
        let final_count = condvar
            .wait_timeout_while(count, std::time::Duration::from_secs(2), |c| *c == 1)
            .unwrap();

        // Should go off despite the fact that we didn't re-arm manually.
        expect_eq!(*final_count.0, 2, "Should have fired twice");
    }
}

#[gtest(RustSystemAPITestSuite, CloseSafeTrapWithActiveTrigger)]
fn test_close_trap_with_active_trigger() {
    // Trap must do some lifecycle management/teardown of the pointers it encloses
    // when it is `drop`'d.
    //
    // Additionally we expect remove_trigger to be called on each active trigger,
    // and the associated callback to return TrapError::Cancelled.
    let trap = system::trap::Trap::new(system::trap::RearmingPolicy::Manual)
        .expect("Failed to create safe Trap");
    let (ep_a, _ep_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();

    trap.add_trigger(
        &ep_a,
        system::raw_trap::HandleSignals::READABLE,
        system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
        move |event| {
            expect_eq!(event.result(), Err(system::trap::TrapError::Cancelled));
        },
    );
    drop(trap);
    // `drop` completed without any errors wrt pointer management and such.
}

#[gtest(RustSystemAPITestSuite, TestClearTriggers)]
fn test_trap_clear_triggers() {
    let mut trap = system::trap::Trap::new(system::trap::RearmingPolicy::Manual)
        .expect("Failed to create safe Trap");
    let (ep_a, ep_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();

    // Borrow checker won't let us use a closure here
    macro_rules! add_trigger {
        ($endpoint:expr) => {
            trap.add_trigger(
                &$endpoint,
                system::raw_trap::HandleSignals::READABLE,
                system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
                |event| {
                    expect_eq!(event.result(), Err(system::trap::TrapError::Cancelled));
                },
            )
        };
    }

    let id1 = add_trigger!(ep_a);
    trap.remove_trigger(id1).expect("Failed to remove trigger");

    add_trigger!(ep_a);
    add_trigger!(ep_b);

    trap.clear_triggers();

    add_trigger!(ep_a);
}

#[gtest(RustSystemAPITestSuite, SafeTrapMultipleBlockingEvents)]
fn test_trap_multiple_blocking_events() {
    let trap = system::trap::Trap::new(system::trap::RearmingPolicy::Manual)
        .expect("Failed to create safe Trap");
    const NUM_TRIGGERS: usize = 20; // More than MAX_BLOCKING_EVENTS

    let callback_count = Arc::new(Mutex::new(0));
    let mut endpoints_a = Vec::with_capacity(NUM_TRIGGERS);
    let mut endpoints_b = Vec::with_capacity(NUM_TRIGGERS);

    // 1. Create NUM_TRIGGERS message pipe pairs. For each, add a trigger.
    for i in 0..NUM_TRIGGERS {
        let (ep_a, ep_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();
        let ep_a_arc = Arc::new(ep_a);

        let callback_count_clone = Arc::clone(&callback_count);
        let ep_a_clone = Arc::clone(&ep_a_arc);
        trap.add_trigger(
            &*ep_a_arc,
            system::raw_trap::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
            move |event| {
                match event.result() {
                    Ok(()) => {
                        // Standard behavior we expect for this test.
                        // We are setting 20 triggers so we will expect it to trigger with Ok()
                        // 20 times.
                        let mut count = callback_count_clone.lock().unwrap();
                        *count += 1;
                        ep_a_clone.read().expect("Failed to read from ep_a in callback");
                    }
                    Err(system::trap::TrapError::Cancelled) => {
                        // Do not increase the callback count, as this is not a
                        // callback we're interested in measuring, but don't
                        // panic either. We expect this at the conclusion of our
                        // test, when the Trap is dropped.
                    }
                    Err(system::trap::TrapError::FailedPrecondition) => {
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
        );

        // 2. Trigger the READABLE signal on ep_a by writing to ep_b.
        // This creates a blocking event for each trigger.
        let write_result =
            ep_b.write(system::message::RawMojoMessage::new_with_bytes(b"x").unwrap());
        expect_true!(write_result.is_ok());
        endpoints_a.push(ep_a_arc); // Keep ep_a alive
        endpoints_b.push(ep_b); // Keep ep_a alive
    }

    // 3. Call trap.arm(). This should now handle all 20 blocking events.
    trap.arm(system::trap::InitialArmingPolicy::RunTriggersOnBlockingEvents)
        .expect("Trap failed to arm after processing multiple blocking events");

    // 4. Verify that all NUM_TRIGGERS callbacks were executed.
    let final_count = *callback_count.lock().unwrap();
    expect_eq!(final_count, NUM_TRIGGERS, "Not all blocking events were processed");

    // 5. Verify the trap is now genuinely armed by calling arm again.
    // Since all blocking events have been handled, this call should immediately
    // return Armed.
    trap.arm(system::trap::InitialArmingPolicy::RunTriggersOnBlockingEvents)
        .expect("Trap failed to re-arm after clearing all blocking events");
    // Manually drop our trap to ensure teardown behavior is as expected
    // (that is, Cancelled returned harmlessly for the various triggers
    // upon removal.)
    drop(trap);
}

// We test the majority of our trap functionality via MessagePipes.
// These DataPipe tests are thus somewhat redundant, but fine to keep for now.
#[gtest(RustSystemAPITestSuite, DataPipes_RawTrapSignalOnReadableTest)]
fn test_raw_trap_signal_on_readable() {
    // We need a few global values to keep track of our test trap events.
    static TEST_TRAP_EVENT_LIST: LazyLock<Mutex<Vec<system::raw_trap::RawTrapEvent>>> =
        LazyLock::new(|| Mutex::new(Vec::new()));
    static TEST_TRAP_EVENT_COND: LazyLock<Condvar> = LazyLock::new(Condvar::new);

    // Helper handler for testing.
    extern "C" fn test_trap_event_handler(event: &system::raw_trap::RawTrapEvent) {
        // If locking fails, it means
        // another thread panicked. In this case we can  simply do
        // nothing. Note that we cannot panic here since this is called  from
        // C code.
        if let Ok(mut list) = TEST_TRAP_EVENT_LIST.lock() {
            list.push(event.clone());
            TEST_TRAP_EVENT_COND.notify_all();
        }
    }

    // Helper function for testing.
    fn wait_for_synchronously_delivered_trap_events(
        test_trap_event_list: &Mutex<Vec<system::raw_trap::RawTrapEvent>>,
        target_len: usize,
    ) -> Vec<system::raw_trap::RawTrapEvent> {
        let list_guard = test_trap_event_list.lock().unwrap();
        // Because Mojo data pipes are synchronous, we can simply wait behind
        // a condvar.
        let guard =
            TEST_TRAP_EVENT_COND.wait_while(list_guard, |list| list.len() < target_len).unwrap();
        guard.clone()
    }

    // Make a new trap.
    let trap = system::raw_trap::RawTrap::new(test_trap_event_handler).unwrap();

    // Make a data pipe pair and add a trigger to both ends of the pipe.
    let (producer, consumer) = system::data_pipe::create(0).unwrap();
    expect_eq!(
        Ok(()),
        trap.add_trigger(
            &consumer,
            system::raw_trap::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
            1,
        )
    );
    expect_eq!(
        Ok(()),
        trap.add_trigger(
            &producer,
            system::raw_trap::HandleSignals::PEER_CLOSED,
            system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
            2,
        )
    );

    let mut blocking_events_buf = [const { std::mem::MaybeUninit::uninit() }; 16];
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
        let event = &list[0];
        expect_eq!(event.trigger_context(), 1);
        expect_eq!(event.result(), Ok(()));
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
    extern "C" fn test_trap_event_handler(_event: &system::raw_trap::RawTrapEvent) {}
    let trap = system::raw_trap::RawTrap::new(test_trap_event_handler).unwrap();

    // Create a data pipe and add a trigger with a dummy CONTEXT.
    let (consumer, _) = system::data_pipe::create(0).unwrap();
    const CONTEXT: usize = 123;
    expect_eq!(
        trap.add_trigger(
            &consumer,
            system::raw_trap::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
            CONTEXT
        ),
        Ok(())
    );
    expect_eq!(
        trap.add_trigger(
            &consumer,
            system::raw_trap::HandleSignals::READABLE,
            system::raw_trap::TriggerCondition::TriggerWhenSatisfied,
            CONTEXT,
        ),
        Err(system::mojo_types::MojoError::AlreadyExists)
    );

    expect_eq!(trap.remove_trigger(CONTEXT), Ok(()));

    expect_eq!(trap.remove_trigger(CONTEXT), Err(system::mojo_types::MojoError::NotFound));
}

#[gtest(RustSystemAPITestSuite, MakeRegularTrap)]
fn test_make_regular_trap() {
    let _trap = system::trap::Trap::new(system::trap::RearmingPolicy::Manual).unwrap();
}

#[gtest(RustSystemAPITestSuite, ReportBadMessage)]
fn test_report_bad_message() {
    let msg = system::message::RawMojoMessage::new_with_bytes(b"moist").unwrap();

    let err_msg: Arc<Mutex<String>> = Arc::new(Mutex::new("".to_string()));
    let err_msg_clone = err_msg.clone();
    test_util::set_default_process_error_handler(move |msg: &str| {
        *err_msg_clone.try_lock().unwrap() = msg.to_string()
    });

    let _ = msg.report_bad_message("OH NO!");

    // SAFETY: We're single-threaded so this isn't racy
    expect_eq!("OH NO!".to_string(), (*err_msg.try_lock().unwrap()).clone());

    test_util::set_default_process_error_handler(|_| {});
}
