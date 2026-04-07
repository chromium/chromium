// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

use std::sync::{Arc, Condvar, Mutex};

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

#[gtest(RustSystemAPITestSuite, MessagePipes_TrapSignalOnReadableTest)]
fn test_trap_signal_on_readable() {
    let (endpoint_a, endpoint_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();

    // 1. Create the safe Trap.
    let mut trap = system::trap::Trap::new(system::trap::RearmingPolicy::Manual)
        .expect("Failed to create safe Trap");

    // 2. We use a Mutex/Condvar to wait for the event in the main thread.
    let hit_count = Arc::new(Mutex::new(0));
    let condvar = Arc::new(Condvar::new());

    let hit_count_clone = Arc::clone(&hit_count);
    let condvar_clone = Arc::clone(&condvar);

    let _trigger_id = trap.add_trigger(
        &endpoint_a,
        system::trap::HandleSignals::READABLE,
        system::trap::TriggerCondition::TriggerWhenSatisfied,
        move |event| {
            if event.result.is_ok() {
                let mut count = hit_count_clone.lock().unwrap();
                *count += 1;
                condvar_clone.notify_all();
            }
        },
    );

    let hit_count_clone = Arc::clone(&hit_count);
    let condvar_clone = Arc::clone(&condvar);

    let _trigger_id = trap.add_trigger(
        &endpoint_b,
        system::trap::HandleSignals::PEER_CLOSED,
        system::trap::TriggerCondition::TriggerWhenSatisfied,
        move |event| {
            if event.result.is_ok() {
                let mut count = hit_count_clone.lock().unwrap();
                *count += 1;
                condvar_clone.notify_all();
            }
        },
    );

    trap.arm().expect("Failed to arm trap");

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

    // Ditch the message we just sent so there's no longer an event waiting in
    // the trap.
    let _ = endpoint_a.read();

    // Need to re-arm since we specifeid the manual rearming policy
    trap.arm().expect("Failed to arm trap");

    drop(endpoint_a);
    {
        let count = hit_count.lock().unwrap();
        let final_count = condvar
            .wait_timeout_while(count, std::time::Duration::from_secs(2), |c| *c == 1)
            .unwrap();

        expect_eq!(*final_count.0, 2, "Should have fired twice");
    }
}

#[gtest(RustSystemAPITestSuite, MessagePipes_TrapAutoRearmTest)]
fn test_trap_auto_rearm() {
    let (endpoint_a, endpoint_b) = system::message_pipe::MessageEndpoint::create_pipe().unwrap();
    let endpoint_a = Arc::new(endpoint_a);
    let endpoint_a_weak = Arc::downgrade(&endpoint_a);

    // 1. Create the safe Trap.
    let mut trap = system::trap::Trap::new(system::trap::RearmingPolicy::Automatic)
        .expect("Failed to create safe Trap");

    // 2. We use a Mutex/Condvar to wait for the event in the main thread.
    let hit_count = Arc::new(Mutex::new(0));
    let condvar = Arc::new(Condvar::new());

    let hit_count_clone = Arc::clone(&hit_count);
    let condvar_clone = Arc::clone(&condvar);

    let _trigger_id = trap.add_trigger(
        &*endpoint_a,
        system::trap::HandleSignals::READABLE,
        system::trap::TriggerCondition::TriggerWhenSatisfied,
        move |event| {
            if event.result.is_ok() {
                let mut count = hit_count_clone.lock().unwrap();
                *count += 1;
                condvar_clone.notify_all();
            }
            let _ = endpoint_a_weak.upgrade().unwrap().read();
        },
    );

    trap.arm().expect("Failed to arm trap");

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
        system::trap::HandleSignals::READABLE,
        system::trap::TriggerCondition::TriggerWhenSatisfied,
        move |event| {
            expect_eq!(event.result, Err(system::trap::TrapError::Cancelled));
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
                system::trap::HandleSignals::READABLE,
                system::trap::TriggerCondition::TriggerWhenSatisfied,
                |event| {
                    expect_eq!(event.result, Err(system::trap::TrapError::Cancelled));
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
    let mut trap = system::trap::Trap::new(system::trap::RearmingPolicy::Manual)
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
            system::trap::HandleSignals::READABLE,
            system::trap::TriggerCondition::TriggerWhenSatisfied,
            move |event| {
                match event.result {
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
    trap.arm().expect("Trap failed to arm after processing multiple blocking events");

    // 4. Verify that all NUM_TRIGGERS callbacks were executed.
    let final_count = *callback_count.lock().unwrap();
    expect_eq!(final_count, NUM_TRIGGERS, "Not all blocking events were processed");

    // 5. Verify the trap is now genuinely armed by calling arm again.
    // Since all blocking events have been handled, this call should immediately
    // return Armed.
    trap.arm().expect("Trap failed to re-arm after clearing all blocking events");
    // Manually drop our trap to ensure teardown behavior is as expected
    // (that is, Cancelled returned harmlessly for the various triggers
    // upon removal.)
    drop(trap);
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
