// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This ffi module is used to interact with the
//! Mojo C bindings API. The structures below are
//! undocumented because they are pulled exactly
//! from the header files of the Mojo C bindings
//! API which can be found in the Mojo repository[1]
//! under //mojo/public/c/include/mojo/system. Very
//! clear documentation on these structures and
//! functions can be found there. It is not worth
//! elaborating on these all again here.
//!
//! [1] https://github.com/domokit/mojo
//!
//! TODO(crbug.com/40206847):
//! * Remove references to the now-nonexistent mojo Github

pub mod raw_ffi {
    #![allow(dead_code)]
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    include!(env!("BINDGEN_RS_FILE"));
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

pub use types::MojoResultCode;
use types::*;

#[allow(non_camel_case_types)]
pub type c_void = std::ffi::c_void;

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
    ($name:ident, $( $mem:ident : $t:ty ),*) => {
        #[repr(transparent)]
        pub struct $name(raw_ffi::$name);

        impl $name {
            #![allow(dead_code)]

            pub fn new($($mem : $t),*) -> $name {
                $name(raw_ffi::$name {
                    struct_size: ::std::mem::size_of::<$name>() as u32,
                    $($mem : $mem),*
                })
            }

            // Get an immutable pointer to the wrapped FFI struct to pass to
            // C functions.
            pub fn inner_ptr(&self) -> *const raw_ffi::$name {
              // SAFETY: $name is a repr(transparent) wrapper around
              // raw_ffi::$name
              self as *const _ as *const _
            }

            // Get a mutable pointer to the wrapped FFI struct to pass to
            // C functions.
            pub fn inner_mut_ptr(&mut self) -> *mut raw_ffi::$name {
              // SAFETY: $name is a repr(transparent) wrapper around
              // raw_ffi::$name
              self as *mut _  as *mut _
            }
        }

        impl std::ops::Deref for $name {
          type Target = raw_ffi::$name;

          fn deref(&self) -> &Self::Target {
            &self.0
          }
        }

        impl std::ops::DerefMut for $name {
          fn deref_mut(&mut self) -> &mut Self::Target {
            &mut self.0
          }
        }
    }
}

declare_mojo_options!(MojoCreateSharedBufferOptions, flags: MojoCreateSharedBufferFlags);
declare_mojo_options!(MojoGetBufferInfoOptions, flags: MojoGetBufferInfoFlags);
declare_mojo_options!(MojoMapBufferOptions, flags: MojoMapBufferFlags);
declare_mojo_options!(MojoDuplicateBufferHandleOptions, flags: MojoDuplicateBufferHandleFlags);
declare_mojo_options!(MojoSharedBufferInfo, size: u64);
declare_mojo_options!(
    MojoCreateDataPipeOptions,
    flags: MojoCreateDataPipeFlags,
    element_num_bytes: u32,
    capacity_num_bytes: u32
);
declare_mojo_options!(MojoWriteDataOptions, flags: MojoWriteDataFlags);
declare_mojo_options!(MojoBeginWriteDataOptions, flags: MojoBeginWriteDataFlags);
declare_mojo_options!(MojoEndWriteDataOptions, flags: MojoEndWriteDataFlags);
declare_mojo_options!(MojoReadDataOptions, flags: MojoReadDataFlags);
declare_mojo_options!(MojoBeginReadDataOptions, flags: MojoBeginReadDataFlags);
declare_mojo_options!(MojoEndReadDataOptions, flags: MojoEndReadDataFlags);
declare_mojo_options!(MojoCreateMessagePipeOptions, flags: MojoCreateMessagePipeFlags);
declare_mojo_options!(MojoWriteMessageOptions, flags: MojoWriteMessageFlags);
declare_mojo_options!(MojoReadMessageOptions, flags: MojoReadMessageFlags);
declare_mojo_options!(MojoCreateMessageOptions, flags: MojoCreateMessageFlags);
declare_mojo_options!(MojoAppendMessageDataOptions, flags: MojoAppendMessageDataFlags);
declare_mojo_options!(MojoGetMessageDataOptions, flags: MojoGetMessageDataFlags);
declare_mojo_options!(MojoCreateTrapOptions, flags: MojoCreateTrapFlags);
declare_mojo_options!(MojoAddTriggerOptions, flags: MojoAddTriggerFlags);
declare_mojo_options!(MojoRemoveTriggerOptions, flags: MojoRemoveTriggerFlags);
declare_mojo_options!(MojoArmTrapOptions, flags: MojoArmTrapFlags);

pub use raw_ffi::MojoAddTrigger;
pub use raw_ffi::MojoAppendMessageData;
pub use raw_ffi::MojoArmTrap;
pub use raw_ffi::MojoBeginReadData;
pub use raw_ffi::MojoBeginWriteData;
pub use raw_ffi::MojoClose;
pub use raw_ffi::MojoCreateDataPipe;
pub use raw_ffi::MojoCreateMessage;
pub use raw_ffi::MojoCreateMessagePipe;
pub use raw_ffi::MojoCreateSharedBuffer;
pub use raw_ffi::MojoCreateTrap;
pub use raw_ffi::MojoDestroyMessage;
pub use raw_ffi::MojoDuplicateBufferHandle;
pub use raw_ffi::MojoEndReadData;
pub use raw_ffi::MojoEndWriteData;
pub use raw_ffi::MojoGetBufferInfo;
pub use raw_ffi::MojoGetMessageData;
pub use raw_ffi::MojoGetTimeTicksNow;
pub use raw_ffi::MojoMapBuffer;
pub use raw_ffi::MojoQueryHandleSignalsState;
pub use raw_ffi::MojoReadData;
pub use raw_ffi::MojoReadMessage;
pub use raw_ffi::MojoRemoveTrigger;
pub use raw_ffi::MojoUnmapBuffer;
pub use raw_ffi::MojoWriteData;
pub use raw_ffi::MojoWriteMessage;

/// Exposed for tests only. Note that calling this function means the Mojo
/// embedder target must be linked in.
pub use raw_ffi::MojoEmbedderSetSystemThunks;
pub use raw_ffi::MojoSystemThunks2;
