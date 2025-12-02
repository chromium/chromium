// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rust_gtest_interop::prelude::*;

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
