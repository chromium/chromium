// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Utilities to support testing Mojo clients and the Mojo system implementation
//! itself.

chromium::import! {
    "//mojo/public/rust:mojo_system" as system;
}

macro_rules! gen_panic_stub {
    ($name:ident $(, $arg:ident : $arg_ty:ty)*) => {
        pub extern "C" fn $name ($($arg : $arg_ty),*) -> MojoResultCode {
            unimplemented!(concat!("test stub not implemented for ", stringify!($name)))
        }
    }
}

/// Define safe but non-functional stubs for Mojo calls. These provide safe
/// behavior for tests that hold Mojo handles but don't use them for anything
/// real. Currently, only freeing handles does anything, and all other stubs
/// simply panic. Tests are free to use any integer for a handle when using this
/// implementation.
mod stubs {
    #![allow(non_snake_case)]
    #![allow(unused_variables)]

    use system::ffi_for_testing::raw_ffi::*;
    use system::ffi_for_testing::{c_void, MojoResultCode};

    gen_panic_stub!(
        AddTrigger,
        trap_handle: MojoHandle,
        handle: MojoHandle,
        signals: MojoHandleSignals,
        condition: MojoTriggerCondition,
        context: usize,
        options: *const MojoAddTriggerOptions
    );
    gen_panic_stub!(
        AppendMessageData,
        message: MojoMessageHandle,
        additional_payload_size: u32,
        handles: *const MojoHandle,
        num_handles: u32,
        options: *const MojoAppendMessageDataOptions,
        buffer: *mut *mut c_void,
        buffer_size: *mut u32
    );
    gen_panic_stub!(
        ArmTrap,
        trap_handle: MojoHandle,
        options: *const MojoArmTrapOptions,
        num_blocking_events: *mut u32,
        blocking_events: *mut MojoTrapEvent
    );
    gen_panic_stub!(
        BeginReadData,
        handle: MojoHandle,
        options: *const MojoBeginReadDataOptions,
        buffer: *mut *const c_void,
        elements: *mut u32
    );
    gen_panic_stub!(
        BeginWriteData,
        handle: MojoHandle,
        options: *const MojoBeginWriteDataOptions,
        buffer: *mut *mut c_void,
        elements: *mut u32
    );
    gen_panic_stub!(Close, handle: MojoHandle);
    gen_panic_stub!(
        CreateDataPipe,
        options: *const MojoCreateDataPipeOptions,
        handle1: *mut MojoHandle,
        handle2: *mut MojoHandle
    );
    gen_panic_stub!(
        CreateMessage,
        options: *const MojoCreateMessageOptions,
        message: *mut MojoMessageHandle
    );
    gen_panic_stub!(
        CreateMessagePipe,
        options: *const MojoCreateMessagePipeOptions,
        handle1: *mut MojoHandle,
        handle2: *mut MojoHandle
    );
    gen_panic_stub!(
        CreateSharedBuffer,
        num_bytes: u64,
        options: *const MojoCreateSharedBufferOptions,
        handle: *mut MojoHandle
    );
    gen_panic_stub!(
        CreateTrap,
        handler: MojoTrapEventHandler,
        options: *const MojoCreateTrapOptions,
        handle: *mut MojoHandle
    );
    gen_panic_stub!(DestroyMessage, handle: MojoMessageHandle);
    gen_panic_stub!(
        DuplicateBufferHandle,
        handle: MojoHandle,
        options: *const MojoDuplicateBufferHandleOptions,
        new_handle: *mut MojoHandle
    );
    gen_panic_stub!(
        EndReadData,
        handle: MojoHandle,
        elements: u32,
        options: *const MojoEndReadDataOptions
    );
    gen_panic_stub!(
        EndWriteData,
        handle: MojoHandle,
        elements: u32,
        options: *const MojoEndWriteDataOptions
    );
    gen_panic_stub!(
        GetBufferInfo,
        handle: MojoHandle,
        options: *const MojoGetBufferInfoOptions,
        info: *mut MojoSharedBufferInfo
    );
    gen_panic_stub!(
        GetMessageData,
        handle: MojoMessageHandle,
        options: *const MojoGetMessageDataOptions,
        buffer: *mut *mut c_void,
        num_bytes: *mut u32,
        handles: *mut MojoHandle,
        num_handles: *mut u32
    );
    gen_panic_stub!(
        MapBuffer,
        handle: MojoHandle,
        offset: u64,
        bytes: u64,
        options: *const MojoMapBufferOptions,
        buffer: *mut *mut c_void
    );
    gen_panic_stub!(
        QueryHandleSignalsState,
        handle: MojoHandle,
        signals_state: *mut MojoHandleSignalsState
    );
    gen_panic_stub!(
        ReadData,
        handle: MojoHandle,
        options: *const MojoReadDataOptions,
        elements: *mut c_void,
        num_elements: *mut u32
    );
    gen_panic_stub!(
        ReadMessage,
        handle: MojoHandle,
        options: *const MojoReadMessageOptions,
        message: *mut MojoMessageHandle
    );
    gen_panic_stub!(
        RemoveTrigger,
        handle: MojoHandle,
        context: usize,
        options: *const MojoRemoveTriggerOptions
    );
    gen_panic_stub!(UnmapBuffer, buffer: *mut c_void);
    gen_panic_stub!(
        WriteData,
        handle: MojoHandle,
        elements: *const c_void,
        num_elements: *mut u32,
        options: *const MojoWriteDataOptions
    );
    gen_panic_stub!(
        WriteMessage,
        handle: MojoHandle,
        message: MojoMessageHandle,
        options: *const MojoWriteMessageOptions
    );

    pub extern "C" fn GetTimeTicksNow() -> MojoTimeTicks {
        0
    }
}

/// Instead of the Mojo core implementation, use non-functional stubs for API
/// calls.
///
/// # Safety
///
/// This may only be called once. Mojo cannot be initialized before or after in
/// the same process, ever.
pub unsafe fn set_stub_thunks() {
    let mut thunks: system::ffi_for_testing::MojoSystemThunks2 =
        unsafe { std::mem::MaybeUninit::zeroed().assume_init() };

    macro_rules! set_thunks {
        ($($name:ident),+ $(,)?) => { $(thunks.$name = Some(stubs::$name));* }
    }

    set_thunks!(
        AddTrigger,
        AppendMessageData,
        ArmTrap,
        BeginReadData,
        BeginWriteData,
        Close,
        CreateDataPipe,
        CreateMessage,
        CreateMessagePipe,
        CreateSharedBuffer,
        CreateTrap,
        DestroyMessage,
        DuplicateBufferHandle,
        EndReadData,
        EndWriteData,
        GetBufferInfo,
        GetMessageData,
        MapBuffer,
        QueryHandleSignalsState,
        ReadData,
        ReadMessage,
        RemoveTrigger,
        UnmapBuffer,
        WriteData,
        WriteMessage,
        GetTimeTicksNow,
    );

    thunks.size = std::mem::size_of_val(&thunks) as u32;

    unsafe {
        set_thunks(thunks);
    }
}

/// Set custom thunks to back Mojo API calls. Mojo core cannot be initialized
/// before or after this call, so it precludes use in any process that will at
/// some point need to use real Mojo functionality. Ideally, this would never be
/// used and downstream code would be able to write tests that don't need a fake
/// Mojo implementation, but here we are.
///
/// # Safety
///
/// Must be called no more than once, including the underlying functions from
/// outside Rust. Mojo cannot be initialized in the same process before or after
/// this call.
pub unsafe fn set_thunks(thunks: system::ffi_for_testing::MojoSystemThunks2) {
    unsafe {
        system::ffi_for_testing::MojoEmbedderSetSystemThunks(&thunks as *const _);
    }
}
