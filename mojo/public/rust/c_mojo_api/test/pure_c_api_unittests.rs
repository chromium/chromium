// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use rust_gtest_interop::prelude::*;

chromium::import! {
    "//mojo/public/rust/c_mojo_api" as mojo_ffi;
    "//mojo/public/rust/system/test_util";
}

// This file is meant to mimic the tests in
// //mojo/public/c/system/tests/core_unittest_pure_c.c.
// These tests are thus somewhat redundant, but useful for ensuring that we're
// wrapping certain C API functions in a sensible way.
#[gtest(RustSystemAPITestSuite, MojoTimeTicksTest)]
fn test_ticks() {
    // get_time_ticks_now should increase monotonically.
    let ticks = mojo_ffi::functions::MojoGetTimeTicksNow();
    assert_ne!(ticks, 0);
}

#[gtest(RustSystemAPITestSuite, MojoMessagePipeTest)]
fn test_message_pipe() {
    // Create a new message pipe, which returns two interchangeable endpoint
    // handles.
    let (handle0, handle1) = mojo_ffi::message_pipe::MojoCreateMessagePipe().unwrap();

    // Create a new message object. Initially, it has no payload.
    let mut message =
        mojo_ffi::message::MojoCreateMessage(mojo_ffi::message::CreateMessageFlags::empty());

    let message_contents: u64 = 42424242;
    let message_bytes = message_contents.to_ne_bytes();

    // Append our data payload (an 8-byte integer) to the message.
    // We use COMMIT_SIZE to indicate this is the final write, marking it ready to
    // be sent.
    let bytes_written = mojo_ffi::message::MojoAppendMessageData(
        &mut message,
        mojo_ffi::message::AppendMessageDataFlags::COMMIT_SIZE,
        &message_bytes,
        vec![],
    )
    .unwrap();
    assert_eq!(bytes_written, std::mem::size_of::<u64>());

    // Send the message through the first endpoint. This consumes the message
    // object.
    mojo_ffi::message_pipe::MojoWriteMessage(&handle0, message).unwrap();

    // Read the message from the second endpoint.
    let read_message = mojo_ffi::message_pipe::MojoReadMessage(&handle1).unwrap();

    // Extract the payload data from the received message object.
    let data_status = mojo_ffi::message::MojoGetMessageData(&read_message, None);
    if let mojo_ffi::message::GetMessageDataStatus::Success { bytes, num_handles_written } =
        data_status
    {
        // Verify the payload matches what we originally wrote.
        assert_eq!(bytes.len(), std::mem::size_of::<u64>());
        let read_contents = u64::from_ne_bytes(bytes.try_into().unwrap());
        assert_eq!(read_contents, message_contents);
        assert_eq!(num_handles_written, 0);
    } else {
        panic!("MojoGetMessageData failed");
    }

    // Handles and messages are safely closed/destroyed via their respective
    // Drop implementations.
}

#[gtest(RustSystemAPITestSuite, MojoSharedBufferTest)]
fn test_shared_buffer() {
    let num_bytes: u64 = 1024;

    // Create a shared buffer of 1024 bytes.
    let handle = mojo_ffi::buffer::MojoCreateSharedBuffer(num_bytes).unwrap();

    // Verify that the system reports the correct size for this buffer handle.
    let size = mojo_ffi::buffer::MojoGetBufferInfo(&handle).unwrap();
    assert_eq!(size, num_bytes);

    // Map the memory into the current process's address space.
    let mapped = mojo_ffi::buffer::MojoMapBuffer(&handle, 0, num_bytes as usize).unwrap();

    // The mapped buffer is a raw pointer. We must use `unsafe` to cast it to a
    // slice and mutate it.
    // TODO(crbug.com/529331861): Once we have a safe abstraction around mapped
    // memory, we should use that instead of casting to a raw pointer.
    // SAFETY: Since we know we are the only ones holding a reference to this
    // mapped memory right now, this is safe to do for the length of the slice.
    unsafe {
        let mapped_slice = std::slice::from_raw_parts_mut(mapped, num_bytes as usize);
        mapped_slice[0] = 42;
        mapped_slice[1023] = 43;
    }

    // Explicitly unmap the mapped memory.
    // SAFETY: `mapped` is a valid pointer returned by `MojoMapBuffer` just above,
    // and it has not been unmapped yet.
    unsafe {
        mojo_ffi::buffer::MojoUnmapBuffer(mapped).unwrap();
    }

    // Duplicate the buffer handle. The shared buffer remains alive as long as at
    // least one handle to it exists. We set read_only to false.
    let duplicate = mojo_ffi::buffer::MojoDuplicateBufferHandle(&handle, false).unwrap();

    // Map the newly duplicated handle and verify the memory contents are exactly
    // what we wrote using the original handle.
    let mapped_dup = mojo_ffi::buffer::MojoMapBuffer(&duplicate, 0, num_bytes as usize).unwrap();
    // SAFETY: `mapped_dup` is a valid pointer to memory we just mapped, and we
    // are only creating a temporary read-only slice of the correct size.
    unsafe {
        let mapped_dup_slice = std::slice::from_raw_parts(mapped_dup, num_bytes as usize);
        assert_eq!(mapped_dup_slice[0], 42);
        assert_eq!(mapped_dup_slice[1023], 43);
    }

    // Unmap the duplicated mapping.
    // SAFETY: `mapped_dup` is a valid pointer returned by `MojoMapBuffer` just
    // above, and it has not been unmapped yet.
    unsafe {
        mojo_ffi::buffer::MojoUnmapBuffer(mapped_dup).unwrap();
    }

    // Buffer handles will be safely closed via Drop.
}

#[gtest(RustSystemAPITestSuite, MojoPlatformHandleTest)]
fn test_platform_handle() {
    let temp_dir = std::env::temp_dir();
    let temp_file_path = temp_dir.join("mojo_rust_test_file");
    let file = std::fs::File::create(&temp_file_path).unwrap();

    #[cfg(target_family = "unix")]
    {
        use std::os::fd::OwnedFd;
        let owned = OwnedFd::from(file);
        let platform_handle = mojo_ffi::platform::PlatformHandle::from(owned);

        // Wrap the platform handle
        let mojo_handle = mojo_ffi::platform::MojoWrapPlatformHandle(platform_handle).unwrap();

        // Unwrap the platform handle
        let unwrapped_platform_handle =
            mojo_ffi::platform::MojoUnwrapPlatformHandle(mojo_handle).unwrap();
        let unwrapped_owned = OwnedFd::from(unwrapped_platform_handle);

        // Verify the unwrapped handle works by creating a File from it and writing to
        // it.
        let mut unwrapped_file = std::fs::File::from(unwrapped_owned);
        use std::io::Write;
        unwrapped_file.write_all(b"hello").unwrap();
    }

    #[cfg(target_family = "windows")]
    {
        use std::os::windows::io::OwnedHandle;
        let owned = OwnedHandle::from(file);
        let platform_handle = mojo_ffi::platform::PlatformHandle::from(owned);

        // Wrap the platform handle
        let mojo_handle = mojo_ffi::platform::MojoWrapPlatformHandle(platform_handle).unwrap();

        // Unwrap the platform handle
        let unwrapped_platform_handle =
            mojo_ffi::platform::MojoUnwrapPlatformHandle(mojo_handle).unwrap();
        let unwrapped_owned = OwnedHandle::from(unwrapped_platform_handle);

        // Verify the unwrapped handle works
        let mut unwrapped_file = std::fs::File::from(unwrapped_owned);
        use std::io::Write;
        unwrapped_file.write_all(b"hello").unwrap();
    }

    // Clean up
    let _ = std::fs::remove_file(temp_file_path);
}
