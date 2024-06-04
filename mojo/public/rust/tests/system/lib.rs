// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests all functionality in the system package
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

#![feature(assert_matches)]
#![feature(maybe_uninit_write_slice)]

chromium::import! {
    "//mojo/public/rust:mojo";
    "//mojo/public/rust/tests:test_util";
}

use mojo::system::shared_buffer::{self, SharedBuffer};
use mojo::system::trap::{
    ArmResult, Trap, TrapEvent, TriggerCondition, UnsafeTrap, UnsafeTrapEvent,
};
use mojo::system::{
    self, data_pipe, message_pipe, wait_set, CastHandle, Handle, HandleSignals, MojoResult,
    SignalsState,
};
use rust_gtest_interop::prelude::*;
use test_util::init;

use std::assert_matches::assert_matches;
use std::mem::drop;
use std::string::String;
use std::sync::{Arc, Condvar, Mutex};
use std::thread;
use std::vec::Vec;

#[gtest(MojoSystemTest, Handle)]
fn test() {
    init();

    let sb = SharedBuffer::new(1).unwrap();
    let handle = sb.as_untyped();
    unsafe {
        expect_ne!(handle.get_native_handle(), 0);
        expect_true!(handle.is_valid());
        let mut h2 = system::acquire(handle.get_native_handle());
        expect_true!(h2.is_valid());
        h2.invalidate();
        expect_true!(!h2.is_valid());
    }
}

#[gtest(MojoSystemTest, SharedBuffer)]
fn test() {
    init();

    let bufsize = 100;

    // Create a shared buffer and test round trip through `UntypedHandle`.
    let sb_first = SharedBuffer::new(bufsize).unwrap();
    // Get original native handle to check against.
    let sb_native_handle = sb_first.get_native_handle();

    let sb_untyped = sb_first.as_untyped();
    expect_eq!(sb_untyped.get_native_handle(), sb_native_handle);
    let sb = unsafe { SharedBuffer::from_untyped(sb_untyped) };
    expect_eq!(sb.get_native_handle(), sb_native_handle);

    // Check the reported size is the same as our requested size.
    let size = sb.get_info().unwrap();
    expect_eq!(size, bufsize);

    // Map the buffer.
    let mut buf = sb.map(0, bufsize).unwrap();
    expect_eq!(buf.len(), bufsize as usize);
    buf.write(50, 34);

    // Duplicate it and drop the original handle, which should maintain the
    // `buf` mapping.
    let sb1 = sb.duplicate(shared_buffer::DuplicateFlags::empty()).unwrap();
    drop(sb);

    buf.write(51, 35);

    // Unmap `buf` by dropping it.
    drop(buf);

    // Create a new mapping and check for what we wrote.
    let buf1 = sb1.map(50, 50).unwrap();
    expect_eq!(buf1.len(), 50);
    // verify buffer contents
    expect_eq!(buf1.read(0), 34);
    expect_eq!(buf1.read(1), 35);
}

#[gtest(MojoSystemTest, MessagePipe)]
fn test() {
    init();

    let (end_a, end_b) = message_pipe::create().unwrap();

    // Extract original handle to check against.
    let end_a_native_handle = end_a.get_native_handle();
    // Test casting of handle types.
    let end_a_untyped = end_a.as_untyped();
    expect_eq!(end_a_untyped.get_native_handle(), end_a_native_handle);

    // Test after UntypedHandle round trip.
    let end_a = unsafe { message_pipe::MessageEndpoint::from_untyped(end_a_untyped) };
    expect_eq!(end_a.get_native_handle(), end_a_native_handle);
    let s: SignalsState = end_a.wait(HandleSignals::WRITABLE).satisfied().unwrap();
    expect_true!(s.satisfied().is_writable());
    expect_true!(s.satisfiable().is_readable());
    expect_true!(s.satisfiable().is_writable());
    expect_true!(s.satisfiable().is_peer_closed());

    assert_matches!(end_a.read(), Err(mojo::MojoResult::ShouldWait));
    let hello = "hello".to_string().into_bytes();
    let write_result = end_b.write(&hello, Vec::new());
    expect_eq!(write_result, mojo::MojoResult::Okay);
    let s: SignalsState = end_a.wait(HandleSignals::READABLE).satisfied().unwrap();
    expect_true!(s.satisfied().is_readable());
    expect_true!(s.satisfied().is_writable());
    expect_true!(s.satisfiable().is_readable());
    expect_true!(s.satisfiable().is_writable());
    expect_true!(s.satisfiable().is_peer_closed());

    let (hello_data, _handles) = end_a.read().expect("failed to read from end_a");
    expect_eq!(String::from_utf8(hello_data), Ok("hello".to_string()));

    // Closing one endpoint should be seen by the other.
    drop(end_a);

    let s: SignalsState =
        end_b.wait(HandleSignals::READABLE | HandleSignals::WRITABLE).unsatisfiable().unwrap();
    expect_true!(s.satisfied().is_peer_closed());
    // For some reason QuotaExceeded is also set. TOOD(collinbaker): investigate.
    expect_true!(s.satisfiable().is_peer_closed());
}

#[gtest(MojoSystemTest, DataPipe)]
fn test() {
    init();

    let (consumer, producer) = data_pipe::create_default().unwrap();
    // Extract original handle to check against
    let consumer_native_handle = consumer.get_native_handle();
    let producer_native_handle = producer.get_native_handle();
    // Test casting of handle types
    let consumer_untyped = consumer.as_untyped();
    let producer_untyped = producer.as_untyped();
    expect_eq!(consumer_untyped.get_native_handle(), consumer_native_handle);
    expect_eq!(producer_untyped.get_native_handle(), producer_native_handle);
    let consumer = unsafe { data_pipe::Consumer::<u8>::from_untyped(consumer_untyped) };
    let producer = unsafe { data_pipe::Producer::<u8>::from_untyped(producer_untyped) };
    expect_eq!(consumer.get_native_handle(), consumer_native_handle);
    expect_eq!(producer.get_native_handle(), producer_native_handle);

    // Ensure the producer is writable, and check that we can wait on this
    // (which should return immediately).
    producer.wait(HandleSignals::WRITABLE).satisfied().unwrap();

    // Try writing a message.
    let hello = "hello".to_string().into_bytes();
    let bytes_written = producer.write(&hello, data_pipe::WriteFlags::empty()).unwrap();
    expect_eq!(bytes_written, hello.len());

    // Try reading our message.
    consumer.wait(HandleSignals::READABLE).satisfied().unwrap();
    let data_string =
        String::from_utf8(consumer.read(data_pipe::ReadFlags::empty()).unwrap()).unwrap();
    expect_eq!(data_string, "hello".to_string());

    // Test two-phase read/write, where we acquire a buffer to use then
    // commit the read/write when done.
    let goodbye = "goodbye".to_string().into_bytes();
    let mut write_buf = producer.begin().expect("error on write begin");
    expect_ge!(write_buf.len(), goodbye.len());
    std::mem::MaybeUninit::copy_from_slice(&mut write_buf[0..goodbye.len()], &goodbye);
    // SAFETY: we wrote `goodbye.len()` valid elements to `write_buf`,
    // so they are initialized.
    unsafe {
        write_buf.commit(goodbye.len());
    }

    // Try a two-phase read and check that we get the same result.
    consumer.wait(HandleSignals::READABLE).satisfied().unwrap();
    let read_buf = consumer.begin().expect("error on read begin");

    // Ensure we get an error when attempting another read.
    assert_matches!(consumer.read(data_pipe::ReadFlags::empty()), Err(mojo::MojoResult::Busy));

    // Copy the buffer to ensure we commit the read before asserting.
    let data = read_buf.to_vec();
    read_buf.commit(data.len());

    expect_eq!(data, goodbye);
}

#[gtest(MojoSystemTest, WaitSet)]
fn test() {
    init();

    let mut set = wait_set::WaitSet::new().unwrap();
    let (endpt0, endpt1) = message_pipe::create().unwrap();
    let cookie1 = wait_set::WaitSetCookie(245);
    let cookie2 = wait_set::WaitSetCookie(123);
    let signals = HandleSignals::READABLE;
    expect_eq!(set.add(&endpt0, signals, cookie1), mojo::MojoResult::Okay);
    expect_eq!(set.add(&endpt0, signals, cookie1), mojo::MojoResult::AlreadyExists);
    expect_eq!(set.remove(cookie1), mojo::MojoResult::Okay);
    expect_eq!(set.remove(cookie1), mojo::MojoResult::NotFound);
    expect_eq!(set.add(&endpt0, signals, cookie2), mojo::MojoResult::Okay);
    thread::spawn(move || {
        let hello = "hello".to_string().into_bytes();
        let write_result = endpt1.write(&hello, Vec::new());
        expect_eq!(write_result, mojo::MojoResult::Okay);
    });
    let mut output = Vec::with_capacity(2);
    let result = set.wait_on_set(&mut output);
    expect_eq!(result, mojo::MojoResult::Okay);
    expect_eq!(output.len(), 1);
    expect_eq!(output[0].cookie, cookie2);
    expect_eq!(output[0].wait_result, mojo::MojoResult::Okay);
    expect_true!(output[0].signals_state.satisfied().is_readable());
}

#[gtest(MojoSystemTest, TrapSignalsOnReadable)]
fn test() {
    init();

    // These tests unfortunately need global state, so we have to ensure
    // exclusive access (generally Rust tests run on multiple threads).
    let _test_lock = TRAP_TEST_LOCK.lock().unwrap();

    let trap = UnsafeTrap::new(test_trap_event_handler).unwrap();

    let (cons, prod) = data_pipe::create_default().unwrap();
    expect_eq!(
        MojoResult::Okay,
        trap.add_trigger(
            cons.get_native_handle(),
            HandleSignals::READABLE,
            TriggerCondition::SignalsSatisfied,
            1
        )
    );
    expect_eq!(
        MojoResult::Okay,
        trap.add_trigger(
            prod.get_native_handle(),
            HandleSignals::PEER_CLOSED,
            TriggerCondition::SignalsSatisfied,
            2
        )
    );

    let mut blocking_events_buf = [std::mem::MaybeUninit::uninit(); 16];
    // The trap should arm with no blocking events since nothing should be
    // triggered yet.
    match trap.arm(Some(&mut blocking_events_buf)) {
        ArmResult::Armed => (),
        ArmResult::Blocked(events) => {
            expect_true!(false, "unexpected blocking events {:?}", events)
        }
        ArmResult::Failed(e) => expect_true!(false, "unexpected mojo error {:?}", e),
    }

    // Check that there are no events in the list (though of course this
    // check is uncertain if a race condition bug exists).
    expect_eq!(TRAP_EVENT_LIST.lock().unwrap().len(), 0);

    // Write to `prod` making `cons` readable.
    expect_eq!(prod.write(&[128u8], data_pipe::WriteFlags::empty()).unwrap(), 1);
    {
        let list = wait_for_trap_events(TRAP_EVENT_LIST.lock().unwrap(), 1);
        expect_eq!(list.len(), 1);
        let event = list[0];
        expect_eq!(event.trigger_context(), 1);
        expect_eq!(event.result(), MojoResult::Okay);
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

    // Once the above event has fired, `trap` is disarmed.

    // Re-arming should block and return the event above.
    match trap.arm(Some(&mut blocking_events_buf)) {
        ArmResult::Blocked(events) => {
            let event: &UnsafeTrapEvent = events.get(0).unwrap();
            expect_eq!(event.trigger_context(), 1);
            expect_eq!(event.result(), MojoResult::Okay);
        }
        ArmResult::Armed => expect_true!(false, "expected event did not arrive"),
        ArmResult::Failed(e) => expect_true!(false, "unexpected Mojo error {:?}", e),
    }

    clear_trap_events(1);

    // Read the data so we don't receive the same event again.
    cons.read(data_pipe::ReadFlags::DISCARD).unwrap();
    match trap.arm(Some(&mut blocking_events_buf)) {
        ArmResult::Armed => (),
        ArmResult::Blocked(events) => {
            expect_true!(false, "unexpected blocking events {:?}", events)
        }
        ArmResult::Failed(e) => expect_true!(false, "unexpected Mojo error {:?}", e),
    }

    // Close `prod` making `cons` permanently unreadable.
    drop(prod);

    // Now we should have two events: one to indicate that cons will never
    // be readable again, and one to indicate that `prod` has been closed
    // and removed from the trap.
    {
        let list = wait_for_trap_events(TRAP_EVENT_LIST.lock().unwrap(), 1);
        expect_eq!(list.len(), 2);
        let (event1, event2) = (list[0], list[1]);
        // Sort the events since the ordering isn't deterministic.
        let (cons_event, prod_event) =
            if event1.trigger_context() == 1 { (event1, event2) } else { (event2, event1) };

        // 1. `cons` can no longer be readable.
        expect_eq!(cons_event.trigger_context(), 1);
        expect_eq!(cons_event.result(), MojoResult::FailedPrecondition);
        expect_true!(!cons_event.signals_state().satisfiable().is_readable());

        // 2. `prod` was closed, yielding a `Cancelled` event.
        expect_eq!(prod_event.trigger_context(), 2);
        expect_eq!(prod_event.result(), MojoResult::Cancelled);
    };

    drop(trap);

    // We should have 3 events: the two we saw above, plus one Cancelled
    // event for `prod` corresponding to removing `prod` from `trap` (which
    // happens automatically on `Trap` closure).
    clear_trap_events(3);
}

#[gtest(MojoSystemTest, TrapHandleClosedBeforeArm)]
fn test() {
    init();

    let _test_lock = TRAP_TEST_LOCK.lock().unwrap();

    let trap = UnsafeTrap::new(test_trap_event_handler).unwrap();

    let (cons, _prod) = data_pipe::create_default().unwrap();
    expect_eq!(
        MojoResult::Okay,
        trap.add_trigger(
            cons.get_native_handle(),
            HandleSignals::READABLE,
            TriggerCondition::SignalsSatisfied,
            1
        )
    );

    drop(cons);

    // A cancelled event will be reported even without arming.
    {
        let events = wait_for_trap_events(TRAP_EVENT_LIST.lock().unwrap(), 1);
        expect_eq!(events.len(), 1, "unexpected events {:?}", *events);
        let event = events[0];
        expect_eq!(event.trigger_context(), 1);
        expect_eq!(event.result(), MojoResult::Cancelled);
    }

    drop(trap);
    clear_trap_events(1);
}

#[gtest(MojoSystemTest, SafeTrap)]
fn test() {
    init();

    struct SharedContext {
        events: Mutex<Vec<TrapEvent>>,
        cond: Condvar,
    }

    let handler = |event: &TrapEvent, context: &Arc<SharedContext>| {
        if let Ok(mut events) = context.events.lock() {
            events.push(*event);
            context.cond.notify_all();
        }
    };

    let context = Arc::new(SharedContext { events: Mutex::new(Vec::new()), cond: Condvar::new() });
    let trap = Trap::new(handler).unwrap();

    let (cons, prod) = data_pipe::create_default().unwrap();
    let _cons_token = trap.add_trigger(
        cons.get_native_handle(),
        HandleSignals::READABLE,
        TriggerCondition::SignalsSatisfied,
        context.clone(),
    );
    let _prod_token = trap.add_trigger(
        prod.get_native_handle(),
        HandleSignals::PEER_CLOSED,
        TriggerCondition::SignalsSatisfied,
        context.clone(),
    );

    expect_eq!(trap.arm(), MojoResult::Okay);

    // Make `cons` readable.
    expect_eq!(prod.write(&[128u8], data_pipe::WriteFlags::empty()), Ok(1));
    {
        let mut events =
            context.cond.wait_while(context.events.lock().unwrap(), |e| e.is_empty()).unwrap();
        expect_eq!(events.len(), 1, "unexpected events {:?}", events);
        let event = events[0];
        expect_eq!(event.handle(), cons.get_native_handle());
        expect_eq!(event.result(), MojoResult::Okay);
        expect_true!(
            event.signals_state().satisfied().is_readable(),
            "{:?}",
            event.signals_state()
        );
        events.clear();
    }

    // Close `cons` to get two events: peer closure on `prod`, and Cancelled on
    // `cons`.
    let cons_native = cons.get_native_handle();
    drop(cons);
    {
        // We get the Cancelled event while unarmed.
        let mut events =
            context.cond.wait_while(context.events.lock().unwrap(), |e| e.is_empty()).unwrap();
        expect_eq!(events.len(), 1, "unexpected events {:?}", events);
        let event = events[0];
        expect_eq!(event.handle(), cons_native);
        expect_eq!(event.result(), MojoResult::Cancelled);
        events.clear();
    }

    // When we try to arm, we'll get the `prod` event.
    expect_eq!(trap.arm(), MojoResult::FailedPrecondition);
    {
        let mut events =
            context.cond.wait_while(context.events.lock().unwrap(), |e| e.is_empty()).unwrap();
        expect_eq!(events.len(), 1, "unexpected events {:?}", events);
        let event = events[0];
        expect_eq!(event.handle(), prod.get_native_handle());
        expect_eq!(event.result(), MojoResult::Okay);
        expect_true!(
            event.signals_state().satisfied().is_peer_closed(),
            "{:?}",
            event.signals_state()
        );
        events.clear();
    }
}

fn clear_trap_events(expected_len: usize) {
    let mut list = TRAP_EVENT_LIST.lock().unwrap();
    expect_eq!(list.len(), expected_len, "unexpected events {:?}", *list);
    list.clear();
}

fn wait_for_trap_events(
    guard: std::sync::MutexGuard<'static, Vec<UnsafeTrapEvent>>,
    expected_len: usize,
) -> std::sync::MutexGuard<'static, Vec<UnsafeTrapEvent>> {
    TRAP_EVENT_COND.wait_while(guard, |l| l.len() < expected_len).unwrap()
}

extern "C" fn test_trap_event_handler(event: &UnsafeTrapEvent) {
    // If locking fails, it means another thread panicked. In this case we can
    // simply do nothing. Note that we cannot panic here since this is called
    // from C code.
    if let Ok(mut list) = TRAP_EVENT_LIST.lock() {
        list.push(*event);
        TRAP_EVENT_COND.notify_all();
    }
}

lazy_static::lazy_static! {
    // We need globals for trap tests so we need mutual exclusion.
    static ref TRAP_TEST_LOCK: Mutex<()> = Mutex::new(());
    // The TrapEvents received by `test_trap_event_handler`.
    static ref TRAP_EVENT_LIST: Mutex<Vec<UnsafeTrapEvent>> = Mutex::new(Vec::new());
    static ref TRAP_EVENT_COND: Condvar = Condvar::new();
}
