// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines safe Rust wrappers around the C System API's
//! PlatformHandles, which are themselves wrappers for OS-specific handles (e.g.
//! Unix FDs, Windows HANDLEs, etc).

chromium::import! {
  "//mojo/public/rust/c_mojo_api:mojo_c_system_bindings" as raw_ffi;
}

use crate::handles::UntypedHandle;
use crate::result::*;

#[cfg(target_family = "unix")]
use std::os::fd::{AsRawFd, FromRawFd, IntoRawFd, OwnedFd, RawFd};

#[cfg(target_family = "windows")]
use std::os::windows::io::{AsRawHandle, FromRawHandle, IntoRawHandle, OwnedHandle, RawHandle};

/// Platform-specific type alias for an owned platform handle.
#[cfg(target_family = "unix")]
pub type OwnedPlatformHandle = OwnedFd;

#[cfg(target_family = "windows")]
pub type OwnedPlatformHandle = OwnedHandle;

/// Platform-specific type alias for a raw platform handle.
#[cfg(target_family = "unix")]
pub type RawPlatformHandle = RawFd;

#[cfg(target_family = "windows")]
pub type RawPlatformHandle = RawHandle;

// Bindgen doesn't seem to be able to resolve C-style casts in #define macros,
// e.g. `#define FOO ((FooType)1)`, so we must redfine them here.
// TODO(flowerhack): Should this be guarded by an if-this-then-that in
// mojo/public/c/system/platform_handle.h?
const MOJO_PLATFORM_HANDLE_TYPE_INVALID: raw_ffi::MojoPlatformHandleType = 0;
#[cfg(target_family = "unix")]
const MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR: raw_ffi::MojoPlatformHandleType = 1;
#[cfg(target_family = "windows")]
// MOJO_PLATFORM_HANDLE_TYPE_MARCH_PORT is skipped, as it is deprecated.
const MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE: raw_ffi::MojoPlatformHandleType = 3;
// TODO(flowerhack): implement FUCHSIA_HANDLE and MACH_SEND_RIGHT and BINDER.

/// A wrapper around a native platform handle.
pub struct PlatformHandle {
    inner: OwnedPlatformHandle,
}

impl PlatformHandle {
    /// The OS-specific Mojo type constant for this platform handle.
    #[cfg(target_family = "unix")]
    pub const PLATFORM_HANDLE_TYPE: raw_ffi::MojoPlatformHandleType =
        MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
    #[cfg(target_family = "windows")]
    pub const PLATFORM_HANDLE_TYPE: raw_ffi::MojoPlatformHandleType =
        MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;

    /// Borrows the raw platform handle value.
    pub fn as_raw(&self) -> RawPlatformHandle {
        #[cfg(target_family = "unix")]
        return self.inner.as_raw_fd();
        #[cfg(target_family = "windows")]
        return self.inner.as_raw_handle();
    }

    /// Consumes the platform handle and returns the raw handle value,
    /// transferring ownership.
    pub fn into_raw(self) -> RawPlatformHandle {
        #[cfg(target_family = "unix")]
        return self.inner.into_raw_fd();
        #[cfg(target_family = "windows")]
        return self.inner.into_raw_handle();
    }

    /// Wraps a raw platform handle.
    ///
    /// # Safety
    /// `raw` must be a valid owned OS handle.
    pub unsafe fn from_raw(raw: RawPlatformHandle) -> MojoResult<Self> {
        #[cfg(target_family = "unix")]
        if raw < 0 {
            Err(MojoError::InvalidArgument)
        } else {
            // SAFETY: The caller guarantees `raw` is a valid owned file descriptor.
            Ok(Self { inner: unsafe { OwnedFd::from_raw_fd(raw) } })
        }
        #[cfg(target_family = "windows")]
        if raw.is_null() || raw as isize == -1 {
            Err(MojoError::InvalidArgument)
        } else {
            // SAFETY: The caller guarantees `raw` is a valid owned handle.
            Ok(Self { inner: unsafe { OwnedHandle::from_raw_handle(raw) } })
        }
    }
}

#[cfg(target_family = "unix")]
impl AsRawFd for PlatformHandle {
    fn as_raw_fd(&self) -> RawFd {
        self.as_raw()
    }
}

#[cfg(target_family = "windows")]
impl AsRawHandle for PlatformHandle {
    fn as_raw_handle(&self) -> RawHandle {
        self.as_raw()
    }
}

#[cfg(target_family = "unix")]
impl From<OwnedFd> for PlatformHandle {
    fn from(owned: OwnedFd) -> Self {
        Self { inner: owned }
    }
}

#[cfg(target_family = "unix")]
impl From<PlatformHandle> for OwnedFd {
    fn from(handle: PlatformHandle) -> Self {
        handle.inner
    }
}

#[cfg(target_family = "windows")]
impl From<OwnedHandle> for PlatformHandle {
    fn from(owned: OwnedHandle) -> Self {
        Self { inner: owned }
    }
}

#[cfg(target_family = "windows")]
impl From<PlatformHandle> for OwnedHandle {
    fn from(handle: PlatformHandle) -> Self {
        handle.inner
    }
}

fn new_raw_platform_handle(
    type_: raw_ffi::MojoPlatformHandleType,
    value: u64,
) -> raw_ffi::MojoPlatformHandle {
    raw_ffi::MojoPlatformHandle {
        struct_size: std::mem::size_of::<raw_ffi::MojoPlatformHandle>() as u32,
        type_,
        value,
    }
}

/// Wraps a native platform handle as a Mojo handle which can be transferred
/// over a message pipe. Takes ownership of the underlying native platform
/// object.
///
/// # Possible Error Codes:
/// - `ResourceExhausted`: if the system is out of handles.
/// - `InvalidArgument`: if `platform_handle` was not a valid platform handle.
pub fn MojoWrapPlatformHandle(platform_handle: PlatformHandle) -> MojoResult<UntypedHandle> {
    let mut mojo_handle: raw_ffi::MojoHandle = 0;

    let raw_platform_handle = new_raw_platform_handle(
        PlatformHandle::PLATFORM_HANDLE_TYPE,
        platform_handle.as_raw() as u64,
    );

    let ret = MojoError::result_from_code(
        // SAFETY: Options is allowed to be null; raw_platform_handle and mojo_handle are
        // references.
        unsafe {
            raw_ffi::MojoWrapPlatformHandle(
                &raw_platform_handle,
                std::ptr::null(),
                &mut mojo_handle,
            )
        },
    );

    ret.map(|_| {
        // Ownership transferred to Mojo, so consume the inner handle to prevent it from
        // closing.
        let _ = platform_handle.into_raw();

        // SAFETY: We just got this handle from Mojo.
        unsafe { UntypedHandle::wrap_raw_value(mojo_handle) }
    })
}

/// Unwraps a native platform handle from a Mojo handle. If this call succeeds,
/// ownership of the underlying platform object is assumed by the caller. The
/// Mojo handle is always closed regardless of success or failure.
///
/// # Possible Error Codes:
/// - `InvalidArgument`: if `mojo_handle` was not a valid Mojo handle wrapping a
///   platform handle.
pub fn MojoUnwrapPlatformHandle(mojo_handle: UntypedHandle) -> MojoResult<PlatformHandle> {
    let mut raw_platform_handle = new_raw_platform_handle(MOJO_PLATFORM_HANDLE_TYPE_INVALID, 0);

    // Consume ownership of the mojo handle value, as the C function closes it on
    // both success and failure.
    let raw_mojo_handle = mojo_handle.into_raw_value();

    MojoError::result_from_code(
        // SAFETY: options is allowed to be null; raw_mojo_handle and raw_platform_handle are
        // references.
        unsafe {
            raw_ffi::MojoUnwrapPlatformHandle(
                raw_mojo_handle,
                std::ptr::null(),
                &mut raw_platform_handle,
            )
        },
    )?;

    if raw_platform_handle.type_ != PlatformHandle::PLATFORM_HANDLE_TYPE {
        return Err(MojoError::InvalidArgument);
    }

    // SAFETY: MojoUnwrapPlatformHandle just succeeded, so we can transfer ownership
    // to us.
    unsafe { PlatformHandle::from_raw(raw_platform_handle.value as RawPlatformHandle) }
}
