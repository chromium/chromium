// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests all functionality in the system package
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

use mojo::system::core;
use mojo::system::data_pipe;
use mojo::system::message_pipe;
use mojo::system::shared_buffer;
use mojo::system::trap::{
    ArmResult, Trap, TrapEvent, TriggerCondition, UnsafeTrap, UnsafeTrapEvent,
};
use mojo::system::wait_set;
use mojo::system::{self, CastHandle, Handle, MojoResult, SignalsState};

use std::string::String;
use std::sync::{Arc, Condvar, Mutex};
use std::thread;
use std::vec::Vec;

tests! {
    fn get_time_ticks_now() {
        let x = core::get_time_ticks_now();
        assert!(x >= 10);
    }

    fn handle() {
        let sb = shared_buffer::create(sbflags!(Create::None), 1).unwrap();
        let handle = sb.as_untyped();
        unsafe {
            assert_eq!((handle.get_native_handle() != 0), handle.is_valid());
            assert!(handle.get_native_handle() != 0 && handle.is_valid());
            let mut h2 = system::acquire(handle.get_native_handle());
            assert!(h2.is_valid());
            h2.invalidate();
            assert!(!h2.is_valid());
        }
    }

    fn shared_buffer() {
        let bufsize = 100;
        let sb1;
        {
            let mut buf;
            {
                let sb_c = shared_buffer::create(sbflags!(Create::None), bufsize).unwrap();
                // Extract original handle to check against
                let sb_h = sb_c.get_native_handle();
                // Test casting of handle types
                let sb_u = sb_c.as_untyped();
                assert_eq!(sb_u.get_native_handle(), sb_h);
                let sb = unsafe { shared_buffer::SharedBuffer::from_untyped(sb_u) };
                assert_eq!(sb.get_native_handle(), sb_h);
                // Test map
                buf = sb.map(0, bufsize, sbflags!(Map::None)).unwrap();
                assert_eq!(buf.len(), bufsize as usize);
                // Test get info
                let size = sb.get_info().unwrap();
                assert_eq!(size, bufsize);
                buf.write(50, 34);
                // Test duplicate
                sb1 = sb.duplicate(sbflags!(Duplicate::None)).unwrap();
            }
            // sb gets closed
            buf.write(51, 35);
        }
        // buf just got closed
        // remap to buf1 from sb1
        let buf1 = sb1.map(50, 50, sbflags!(Map::None)).unwrap();
        assert_eq!(buf1.len(), 50);
        // verify buffer contents
        assert_eq!(buf1.read(0), 34);
        assert_eq!(buf1.read(1), 35);
    }

    fn message_pipe() {
        let (endpt, endpt1) = message_pipe::create(mpflags!(Create::None)).unwrap();
        // Extract original handle to check against
        let endpt_h = endpt.get_native_handle();
        // Test casting of handle types
        let endpt_u = endpt.as_untyped();
        assert_eq!(endpt_u.get_native_handle(), endpt_h);
        {
            let endpt0 = unsafe { message_pipe::MessageEndpoint::from_untyped(endpt_u) };
            assert_eq!(endpt0.get_native_handle(), endpt_h);
            {
                let s: SignalsState = endpt0.wait(signals!(Signals::Writable)).satisfied().unwrap();
                assert!(s.satisfied().is_writable());
                assert!(s.satisfiable().is_readable());
                assert!(s.satisfiable().is_writable());
                assert!(s.satisfiable().is_peer_closed());
            }
            match endpt0.read(mpflags!(Read::None)) {
                Ok((_msg, _handles)) => panic!("Read should not have succeeded."),
                Err(r) => assert_eq!(r, mojo::MojoResult::ShouldWait),
            }
            let hello = "hello".to_string().into_bytes();
            let write_result = endpt1.write(&hello, Vec::new(), mpflags!(Write::None));
            assert_eq!(write_result, mojo::MojoResult::Okay);
            {
                let s: SignalsState = endpt0.wait(signals!(Signals::Readable)).satisfied().unwrap();
                assert!(s.satisfied().is_readable());
                assert!(s.satisfied().is_writable());
                assert!(s.satisfiable().is_readable());
                assert!(s.satisfiable().is_writable());
                assert!(s.satisfiable().is_peer_closed());
            }
            let hello_data;
            match endpt0.read(mpflags!(Read::None)) {
                Ok((msg, _handles)) => hello_data = msg,
                Err(r) => panic!("Failed to read message on endpt0, error: {}", r),
            }
            assert_eq!(String::from_utf8(hello_data).unwrap(), "hello".to_string());
        }
        let s: SignalsState = endpt1.wait(signals!(Signals::Readable, Signals::Writable)).unsatisfiable().unwrap();
        assert!(s.satisfied().is_peer_closed());
        // For some reason QuotaExceeded is also set. TOOD(collinbaker): investigate.
        assert!(s.satisfiable().get_bits() & (system::Signals::PeerClosed as u32) > 0);
    }

    fn data_pipe() {
        let (cons0, prod0) = data_pipe::create_default().unwrap();
        // Extract original handle to check against
        let cons_h = cons0.get_native_handle();
        let prod_h = prod0.get_native_handle();
        // Test casting of handle types
        let cons_u = cons0.as_untyped();
        let prod_u = prod0.as_untyped();
        assert_eq!(cons_u.get_native_handle(), cons_h);
        assert_eq!(prod_u.get_native_handle(), prod_h);
        let cons = unsafe { data_pipe::Consumer::<u8>::from_untyped(cons_u) };
        let prod = unsafe { data_pipe::Producer::<u8>::from_untyped(prod_u) };
        assert_eq!(cons.get_native_handle(), cons_h);
        assert_eq!(prod.get_native_handle(), prod_h);
        // Test waiting on producer
        prod.wait(signals!(Signals::Writable)).satisfied().unwrap();
        // Test one-phase read/write.
        // Writing.
        let hello = "hello".to_string().into_bytes();
        let bytes_written = prod.write(&hello, dpflags!(Write::None)).unwrap();
        assert_eq!(bytes_written, hello.len());
        // Reading.
        cons.wait(signals!(Signals::Readable)).satisfied().unwrap();
        let data_string = String::from_utf8(cons.read(dpflags!(Read::None)).unwrap()).unwrap();
        assert_eq!(data_string, "hello".to_string());
        {
            // Test two-phase read/write.
            // Writing.
            let goodbye = "goodbye".to_string().into_bytes();
            let mut write_buf = match prod.begin() {
                Ok(buf) => buf,
                Err(err) => panic!("Error on write begin: {}", err),
            };
            assert!(write_buf.len() >= goodbye.len());
            for i in 0..goodbye.len() {
                write_buf[i] = goodbye[i];
            }
            match write_buf.commit(goodbye.len()) {
                Some((_buf, _err)) => assert!(false),
                None => (),
            }
            // Reading.
            cons.wait(signals!(Signals::Readable)).satisfied().unwrap();
            let mut data_goodbye: Vec<u8> = Vec::with_capacity(goodbye.len());
            {
                let read_buf = match cons.begin() {
                    Ok(buf) => buf,
                    Err(err) => panic!("Error on read begin: {}", err),
                };
                for i in 0..read_buf.len() {
                    data_goodbye.push(read_buf[i]);
                }
                match cons.read(dpflags!(Read::None)) {
                    Ok(_bytes) => assert!(false),
                    Err(r) => assert_eq!(r, mojo::MojoResult::Busy),
                }
                match read_buf.commit(data_goodbye.len()) {
                    Some((_buf, _err)) => assert!(false),
                    None => (),
                }
            }
            assert_eq!(data_goodbye.len(), goodbye.len());
            assert_eq!(String::from_utf8(data_goodbye).unwrap(), "goodbye".to_string());
        }
    }

    fn wait_set() {
        let mut set = wait_set::WaitSet::new(wsflags!(Create::None)).unwrap();
        let (endpt0, endpt1) = message_pipe::create(mpflags!(Create::None)).unwrap();
        let signals = signals!(Signals::Readable);
        let flags = wsflags!(Add::None);
        assert_eq!(set.add(&endpt0, signals, 245, flags), mojo::MojoResult::Okay);
        assert_eq!(set.add(&endpt0, signals, 245, flags), mojo::MojoResult::AlreadyExists);
        assert_eq!(set.remove(245), mojo::MojoResult::Okay);
        assert_eq!(set.remove(245), mojo::MojoResult::NotFound);
        assert_eq!(set.add(&endpt0, signals, 123, flags), mojo::MojoResult::Okay);
        thread::spawn(move || {
            let hello = "hello".to_string().into_bytes();
            let write_result = endpt1.write(&hello, Vec::new(), mpflags!(Write::None));
            assert_eq!(write_result, mojo::MojoResult::Okay);
        });
        let mut output = Vec::with_capacity(2);
        let result = set.wait_on_set(&mut output);
        assert_eq!(result, mojo::MojoResult::Okay);
        assert_eq!(output.len(), 1);
        assert_eq!(output[0].cookie(), 123);
        assert_eq!(output[0].result(), mojo::MojoResult::Okay);
        assert!(output[0].state().satisfied().is_readable());
    }

    fn trap_signals_on_readable_unwritable() {
        // These tests unfortunately need global state, so we have to ensure
        // exclusive access (generally Rust tests run on multiple threads).
        let _test_lock = TRAP_TEST_LOCK.lock().unwrap();

        let trap = UnsafeTrap::new(test_trap_event_handler).unwrap();

        let (cons, prod) = data_pipe::create_default().unwrap();
        assert_eq!(MojoResult::Okay,
            trap.add_trigger(cons.get_native_handle(),
                             signals!(Signals::Readable),
                             TriggerCondition::SignalsSatisfied,
                             1));
        assert_eq!(MojoResult::Okay,
            trap.add_trigger(prod.get_native_handle(),
                             signals!(Signals::Writable),
                             TriggerCondition::SignalsUnsatisfied,
                             2));

        let mut blocking_events_buf = [std::mem::MaybeUninit::uninit(); 16];
        // The trap should arm with no blocking events since nothing should be
        // triggered yet.
        match trap.arm(Some(&mut blocking_events_buf)) {
            ArmResult::Armed => (),
            ArmResult::Blocked(events) => panic!("unexpected blocking events {:?}", events),
            ArmResult::Failed(e) => panic!("unexpected mojo error {:?}", e),
        }

        // Check that there are no events in the list (though of course this
        // check is uncertain if a race condition bug exists).
        assert_eq!(TRAP_EVENT_LIST.lock().unwrap().len(), 0);

        // Write to `prod` making `cons` readable.
        assert_eq!(prod.write(&[128u8], dpflags!(Write::None)).unwrap(), 1);
        {
            let list = wait_for_trap_events(TRAP_EVENT_LIST.lock().unwrap(), 1);
            assert_eq!(list.len(), 1);
            let event = list[0];
            assert_eq!(event.trigger_context(), 1);
            assert_eq!(event.result(), MojoResult::Okay);
            assert!(event.signals_state().satisfiable().is_readable(),
                    "{:?}", event.signals_state());
            assert!(event.signals_state().satisfied().is_readable(),
                    "{:?}", event.signals_state());
        }

        // Once the above event has fired, `trap` is disarmed.

        // Re-arming should block and return the event above.
        match trap.arm(Some(&mut blocking_events_buf)) {
            ArmResult::Blocked(events) => {
                let event: &UnsafeTrapEvent = events.get(0).unwrap();
                assert_eq!(event.trigger_context(), 1);
                assert_eq!(event.result(), MojoResult::Okay);
            }
            ArmResult::Armed => panic!("expected event did not arrive"),
            ArmResult::Failed(e) => panic!("unexpected Mojo error {:?}", e),
        }

        clear_trap_events(1);

        // Read the data so we don't receive the same event again.
        cons.read(dpflags!(Read::Discard)).unwrap();
        match trap.arm(Some(&mut blocking_events_buf)) {
            ArmResult::Armed => (),
            ArmResult::Blocked(events) => panic!("unexpected blocking events {:?}", events),
            ArmResult::Failed(e) => panic!("unexpected Mojo error {:?}", e),
        }

        // Close `cons` making `prod` unwritable.
        drop(cons);

        // Now we should have two events indicating `prod` is not writable.
        {
            let list = wait_for_trap_events(TRAP_EVENT_LIST.lock().unwrap(), 1);
            assert_eq!(list.len(), 2);
            let (event1, event2) = (list[0], list[1]);
            // Sort the events since the ordering isn't deterministic.
            let (cons_event, prod_event) = if event1.trigger_context() == 1 {
                (event1, event2)
            } else {
                (event2, event1)
            };

            // 1. `cons` was closed, yielding a `Cancelled` event.
            assert_eq!(cons_event.trigger_context(), 1);
            assert_eq!(cons_event.result(), MojoResult::Cancelled);

            // 2. `prod`'s trigger condition (being unwritable) was met,
            // yielding a normal event.
            assert_eq!(prod_event.trigger_context(), 2);
            assert_eq!(prod_event.result(), MojoResult::Okay);
            assert!(!prod_event.signals_state().satisfiable().is_writable())
        };

        drop(trap);

        // We should have 3 events: the two we saw above, plus one Cancelled
        // event for `prod` corresponding to removing `prod` from `trap` (which
        // happens automatically on `Trap` closure).
        clear_trap_events(3);
    }

    fn trap_handle_closed_before_arm() {
        let _test_lock = TRAP_TEST_LOCK.lock().unwrap();

        let trap = UnsafeTrap::new(test_trap_event_handler).unwrap();

        let (cons, _prod) = data_pipe::create_default().unwrap();
        assert_eq!(MojoResult::Okay,
            trap.add_trigger(cons.get_native_handle(),
                             signals!(Signals::Readable),
                             TriggerCondition::SignalsSatisfied, 1));

        drop(cons);

        // A cancelled event will be reported even without arming.
        {
            let events = wait_for_trap_events(TRAP_EVENT_LIST.lock().unwrap(), 1);
            assert_eq!(events.len(), 1, "unexpected events {:?}", *events);
            let event = events[0];
            assert_eq!(event.trigger_context(), 1);
            assert_eq!(event.result(), MojoResult::Cancelled);
        }

        drop(trap);
        clear_trap_events(1);
    }

    fn safe_trap() {
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

        let context = Arc::new(SharedContext {
            events: Mutex::new(Vec::new()),
            cond: Condvar::new(),
        });
        let trap = Trap::new(handler).unwrap();

        let (cons, prod) = data_pipe::create_default().unwrap();
        let _cons_token = trap.add_trigger(
            cons.get_native_handle(),
            signals!(Signals::Readable),
            TriggerCondition::SignalsSatisfied,
            context.clone());
        let _prod_token = trap.add_trigger(
            prod.get_native_handle(),
            signals!(Signals::Writable),
            TriggerCondition::SignalsUnsatisfied,
            context.clone());

        assert_eq!(trap.arm(), MojoResult::Okay);

        // Make `cons` readable.
        assert_eq!(prod.write(&[128u8], dpflags!(Write::None)), Ok(1));
        {
            let mut events =
                context.cond.wait_while(context.events.lock().unwrap(), |e| e.is_empty()).unwrap();
            assert_eq!(events.len(), 1, "unexpected events {:?}", events);
            let event = events[0];
            assert_eq!(event.handle(), cons.get_native_handle());
            assert_eq!(event.result(), MojoResult::Okay);
            assert!(event.signals_state().satisfied().is_readable(), "{:?}", event.signals_state());
            events.clear();
        }

        // Close `cons` to get two events: unreadable on `prod`, and Cancelled on `cons`.
        let cons_native = cons.get_native_handle();
        drop(cons);
        {
            // We get the Cancelled event while unarmed.
            let mut events =
                context.cond.wait_while(context.events.lock().unwrap(), |e| e.is_empty()).unwrap();
            assert_eq!(events.len(), 1, "unexpected events {:?}", events);
            let event = events[0];
            assert_eq!(event.handle(), cons_native);
            assert_eq!(event.result(), MojoResult::Cancelled);
            events.clear();
        }

        // When we try to arm, we'll get the `prod` event.
        assert_eq!(trap.arm(), MojoResult::FailedPrecondition);
        {
            let mut events =
                context.cond.wait_while(context.events.lock().unwrap(), |e| e.is_empty()).unwrap();
            assert_eq!(events.len(), 1, "unexpected events {:?}", events);
            let event = events[0];
            assert_eq!(event.handle(), prod.get_native_handle());
            assert_eq!(event.result(), MojoResult::Okay);
            assert!(!event.signals_state().satisfied().is_writable(),
                    "{:?}", event.signals_state());
            events.clear();
        }
     }
}

fn clear_trap_events(expected_len: usize) {
    let mut list = TRAP_EVENT_LIST.lock().unwrap();
    assert_eq!(list.len(), expected_len, "unexpected events {:?}", *list);
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
