// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Tests all functionality in the system package
//!
//! Test failure is defined as the function returning via panicking
//! and the result being caught in the test! macro. If a test function
//! returns without panicking, it is assumed to pass.

use mojo::system;
use mojo::system::core;
use mojo::system::data_pipe;
use mojo::system::message_pipe;
use mojo::system::shared_buffer;
use mojo::system::wait_set;
use mojo::system::{CastHandle, Handle};

use std::string::String;
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
                let (s, r) = endpt0.wait(signals!(Signals::Writable));
                assert_eq!(r, mojo::MojoResult::Okay);
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
                let (s, r) = endpt0.wait(signals!(Signals::Readable));
                assert_eq!(r, mojo::MojoResult::Okay);
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
        let (s, r) = endpt1.wait(signals!(Signals::Readable, Signals::Writable));
        assert_eq!(r, mojo::MojoResult::FailedPrecondition);
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
        {
            let (_s, r) = prod.wait(signals!(Signals::Writable));
            assert_eq!(r, mojo::MojoResult::Okay);
        }
        // Test one-phase read/write.
        // Writing.
        let hello = "hello".to_string().into_bytes();
        let bytes_written = prod.write(&hello, dpflags!(Write::None)).unwrap();
        assert_eq!(bytes_written, hello.len());
        // Reading.
        {
            let (_s, r) = cons.wait(signals!(Signals::Readable));
            assert_eq!(r, mojo::MojoResult::Okay);
        }
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
            {
                let (_s, r) = cons.wait(signals!(Signals::Readable));
                assert_eq!(r, mojo::MojoResult::Okay);
            }
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
}
