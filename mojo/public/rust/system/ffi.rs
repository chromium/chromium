//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We're re-exporting C functions with a different naming convention
#![allow(non_snake_case)]

chromium::import! {
  pub "//mojo/public/rust:mojo_c_system_bindings" as raw_ffi;
}

pub mod types {
    //! Defines some C-compatible types for the ffi layer of
    //! the bindings.
    use super::raw_ffi;

    pub use raw_ffi::MojoAddTriggerFlags;
    pub use raw_ffi::MojoAppendMessageDataFlags;
    pub use raw_ffi::MojoArmTrapFlags;
    pub use raw_ffi::MojoBeginReadDataFlags;
    pub use raw_ffi::MojoBeginWriteDataFlags;
    pub use raw_ffi::MojoCreateDataPipeFlags;
    pub use raw_ffi::MojoCreateMessageFlags;
    pub use raw_ffi::MojoCreateMessagePipeFlags;
    pub use raw_ffi::MojoCreateSharedBufferFlags;
    pub use raw_ffi::MojoCreateTrapFlags;
    pub use raw_ffi::MojoDuplicateBufferHandleFlags;
    pub use raw_ffi::MojoEndReadDataFlags;
    pub use raw_ffi::MojoEndWriteDataFlags;
    pub use raw_ffi::MojoGetBufferInfoFlags;
    pub use raw_ffi::MojoGetMessageDataFlags;
    pub use raw_ffi::MojoHandle;
    pub use raw_ffi::MojoHandleSignals;
    pub use raw_ffi::MojoHandleSignalsState;
    pub use raw_ffi::MojoMapBufferFlags;
    pub use raw_ffi::MojoMessageHandle;
    pub use raw_ffi::MojoReadDataFlags;
    pub use raw_ffi::MojoReadMessageFlags;
    pub use raw_ffi::MojoRemoveTriggerFlags;
    pub use raw_ffi::MojoTimeTicks;
    pub use raw_ffi::MojoTrapEventFlags;
    pub use raw_ffi::MojoTrapEventHandler;
    pub use raw_ffi::MojoTriggerCondition;
    pub use raw_ffi::MojoWriteDataFlags;
    pub use raw_ffi::MojoWriteMessageFlags;
    pub type MojoResultCode = raw_ffi::MojoResult;
}

// SAFETY: The `num_bytes` argument to this function must not be null.
// Additionally the `data` argument must have at least `num_bytes` of
// valid memory. Additionally, for thread safety, one must have exclusive
// access to `data_pipe_producer_handle`.
pub use raw_ffi::MojoWriteData;

// SAFETY: The `num_bytes` argument to this function must not be null.
pub use raw_ffi::MojoReadData;

// SAFETY: The handle ptr is allowed to be null so long as the number of handles
// is 0. The option and buffer pointers are always allowed to be null.
pub use raw_ffi::MojoAppendMessageData;

// FOR_RELEASE: These safe wrappers are good for users, but they prevent
// rust-analyzer from seeing the comments on the original C function, which are
// very useful :/

// Safe wrapper around MojoDestroyMessage
pub fn MojoDestroyMessage(message: raw_ffi::MojoMessageHandle) -> raw_ffi::MojoResult {
    // SAFETY: This function is safe to call (but bindgen doesn't know that)
    // If any arguments are invalid, that will be reflected in the result.
    unsafe { raw_ffi::MojoDestroyMessage(message) }
}

// Safe wrapper around MojoCreateMessage. Returns invalid_argument if `message`
// is null.
pub fn MojoCreateMessage(
    options: *const raw_ffi::MojoCreateMessageOptions,
    message: *mut types::MojoMessageHandle,
) -> raw_ffi::MojoResult {
    // SAFETY: This function is safe to call (but bindgen doesn't know that)
    // If any arguments are invalid, that will be reflected in the result.
    unsafe { raw_ffi::MojoCreateMessage(options, message) }
}

// Safe wrapper around MojoWriteMessage
pub fn MojoWriteMessage(
    message_pipe_handle: types::MojoHandle,
    message: types::MojoMessageHandle,
    options: *const raw_ffi::MojoWriteMessageOptions,
) -> raw_ffi::MojoResult {
    // SAFETY: This function is safe to call (but bindgen doesn't know that)
    // If any arguments are invalid, that will be reflected in the result.
    // In particular, `options` is explicitly allowed to be null.
    unsafe { raw_ffi::MojoWriteMessage(message_pipe_handle, message, options) }
}

// SAFETY: The `buffer` and `num_bytes` arguments must not be null.
// The `options` and `num_handles` arguments may be null.
// The `handles` argument may be null only if `num_handles` is null or
// `*num_handles` is 0.
// If `handles` is non-null and `*num_handles` > 0, `handles` must point to a
// valid buffer with capacity to hold at least `*num_handles` handles.
pub use raw_ffi::MojoGetMessageData;

pub use raw_ffi::MojoAddTrigger;
pub use raw_ffi::MojoArmTrap;
pub use raw_ffi::MojoClose;
pub use raw_ffi::MojoCreateDataPipe;
pub use raw_ffi::MojoCreateMessagePipe;
pub use raw_ffi::MojoCreateTrap;
pub use raw_ffi::MojoGetTimeTicksNow;
pub use raw_ffi::MojoHandleSignalsState as SignalsState;
pub use raw_ffi::MojoQueryHandleSignalsState;
pub use raw_ffi::MojoReadMessage;
pub use raw_ffi::MojoRemoveTrigger;
pub use raw_ffi::MojoTrapEvent;
pub use types::MojoResultCode;

// Most FFI functions take an options struct as input which we get from bindgen.
// Each one contains a `struct_size` member for versioning. We want to make a
// 'newtype' wrapper for each that manages the struct_size as well as adds a
// `new()` function for construction.
//
// To reduce boilerplate we use a macro.
//
// The generated structs contain methods to get raw pointers for passing to FFI
// functions. Note the FFI functions don't require the structs to live beyond
// each call.
macro_rules! declare_mojo_options {
    ($struct_name:ident, $( $field_name:ident : $field_type:ty ),*) => {
        #[repr(transparent)]
        pub struct $struct_name(raw_ffi::$struct_name);

        impl $struct_name {
            pub fn new($($field_name : $field_type),*) -> $struct_name {
                $struct_name(raw_ffi::$struct_name {
                    struct_size: ::std::mem::size_of::<$struct_name>() as u32,
                    $($field_name),*
                })
            }

            // Get an immutable pointer to the wrapped FFI struct to pass to
            // C functions.
            pub fn as_ptr(&self) -> *const raw_ffi::$struct_name {
              // $struct_name is a repr(transparent) wrapper around raw_ffi::$struct_name
              self as *const _ as *const _
            }

            // Get a mutable pointer to the wrapped FFI struct to pass to
            // C functions.
            pub fn as_mut_ptr(&mut self) -> *mut raw_ffi::$struct_name {
              // $struct_name is a repr(transparent) wrapper around raw_ffi::$struct_name
              self as *mut _  as *mut _
            }
        }

        impl std::ops::Deref for $struct_name {
          type Target = raw_ffi::$struct_name;

          fn deref(&self) -> &Self::Target {
            &self.0
          }
        }

        impl std::ops::DerefMut for $struct_name {
          fn deref_mut(&mut self) -> &mut Self::Target {
            &mut self.0
          }
        }
    }
}

declare_mojo_options!(MojoAppendMessageDataOptions, flags: types::MojoAppendMessageDataFlags);
declare_mojo_options!(MojoCreateMessagePipeOptions, flags: types::MojoCreateMessagePipeFlags);
declare_mojo_options!(MojoWriteMessageOptions, flags: types::MojoWriteMessageFlags);
declare_mojo_options!(MojoReadDataOptions, flags: types::MojoReadDataFlags);
declare_mojo_options!(MojoWriteDataOptions, flags: types::MojoWriteDataFlags);
declare_mojo_options!(MojoCreateTrapOptions, flags: types::MojoCreateTrapFlags);
declare_mojo_options!(MojoAddTriggerOptions,flags: types::MojoAddTriggerFlags);
declare_mojo_options!(MojoRemoveTriggerOptions, flags: types::MojoRemoveTriggerFlags);
declare_mojo_options!(MojoArmTrapOptions, flags: types::MojoArmTrapFlags);
declare_mojo_options!(MojoGetMessageDataOptions, flags: types::MojoGetMessageDataFlags);

declare_mojo_options!(
    MojoCreateDataPipeOptions,
    flags: types::MojoCreateDataPipeFlags,
    element_num_bytes: u32,
    capacity_num_bytes: u32
);
