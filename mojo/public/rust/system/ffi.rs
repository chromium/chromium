//Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

pub use types::MojoResultCode;
