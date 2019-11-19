// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a C++ wrapping around the Mojo C API for platform handles,
// replacing the prefix of "Mojo" with a "mojo" namespace.
//
// Please see "mojo/public/c/system/platform_handle.h" for complete
// documentation of the API.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_PLATFORM_HANDLE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_PLATFORM_HANDLE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/system_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace mojo {

#if defined(OS_WIN)
const MojoPlatformHandleType kPlatformFileHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;

const MojoPlatformHandleType kPlatformSharedBufferHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;

#elif defined(OS_FUCHSIA)
const MojoPlatformHandleType kPlatformFileHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
const MojoPlatformHandleType kPlatformSharedBufferHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE;

#elif defined(OS_POSIX)
const MojoPlatformHandleType kPlatformFileHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;

#if defined(OS_MACOSX) && !defined(OS_IOS)
const MojoPlatformHandleType kPlatformSharedBufferHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT;
#else
const MojoPlatformHandleType kPlatformSharedBufferHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

#endif  // defined(OS_WIN)

// Used to specify the protection status of a base::SharedMemoryHandle memory
// handle wrapped or unwrapped by mojo::WrapSharedMemoryHandle or
// mojo::UnwrapSharedMemoryHandle below. See those functions for additional
// details.
enum class UnwrappedSharedMemoryHandleProtection {
  // Indicates that the base::SharedMemoryHandle supports being mapped to
  // writable memory regions.
  kReadWrite,

  // Indicates that the base::SharedMemoryHandle supports being mapped only to
  // read-only memory regions.
  kReadOnly,
};

// Wraps and unwraps base::subtle::PlatformSharedMemoryRegions. This should be
// used only while transitioning from the legacy shared memory API. In new code
// only base::*SharedMemoryRegion should be used instead.
MOJO_CPP_SYSTEM_EXPORT ScopedSharedBufferHandle
WrapPlatformSharedMemoryRegion(base::subtle::PlatformSharedMemoryRegion region);
MOJO_CPP_SYSTEM_EXPORT base::subtle::PlatformSharedMemoryRegion
UnwrapPlatformSharedMemoryRegion(ScopedSharedBufferHandle mojo_handle);

// Wraps a PlatformHandle from the C++ platform support library as a Mojo
// handle.
MOJO_CPP_SYSTEM_EXPORT ScopedHandle WrapPlatformHandle(PlatformHandle handle);

// Unwraps a Mojo handle to a PlatformHandle object from the C++ platform
// support library.
MOJO_CPP_SYSTEM_EXPORT PlatformHandle UnwrapPlatformHandle(ScopedHandle handle);

// Wraps a PlatformFile as a Mojo handle. Takes ownership of the file object.
// If |platform_file| is valid, this will return a valid handle.
MOJO_CPP_SYSTEM_EXPORT
ScopedHandle WrapPlatformFile(base::PlatformFile platform_file);

// Unwraps a PlatformFile from a Mojo handle.
MOJO_CPP_SYSTEM_EXPORT
MojoResult UnwrapPlatformFile(ScopedHandle handle, base::PlatformFile* file);

// Helpers for wrapping and unwrapping new base shared memory API primitives.
// If the input |region| is valid for the Wrap* functions, they will always
// succeed and return a valid Mojo shared buffer handle.

MOJO_CPP_SYSTEM_EXPORT ScopedSharedBufferHandle
WrapReadOnlySharedMemoryRegion(base::ReadOnlySharedMemoryRegion region);

MOJO_CPP_SYSTEM_EXPORT ScopedSharedBufferHandle
WrapUnsafeSharedMemoryRegion(base::UnsafeSharedMemoryRegion region);

MOJO_CPP_SYSTEM_EXPORT ScopedSharedBufferHandle
WrapWritableSharedMemoryRegion(base::WritableSharedMemoryRegion region);

MOJO_CPP_SYSTEM_EXPORT base::ReadOnlySharedMemoryRegion
UnwrapReadOnlySharedMemoryRegion(ScopedSharedBufferHandle handle);

MOJO_CPP_SYSTEM_EXPORT base::UnsafeSharedMemoryRegion
UnwrapUnsafeSharedMemoryRegion(ScopedSharedBufferHandle handle);

MOJO_CPP_SYSTEM_EXPORT base::WritableSharedMemoryRegion
UnwrapWritableSharedMemoryRegion(ScopedSharedBufferHandle handle);

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Wraps a mach_port_t as a Mojo handle. This takes a reference to the
// Mach port.
MOJO_CPP_SYSTEM_EXPORT ScopedHandle WrapMachPort(mach_port_t port);

// Unwraps a mach_port_t from a Mojo handle. The caller gets ownership of the
// Mach port.
MOJO_CPP_SYSTEM_EXPORT MojoResult UnwrapMachPort(ScopedHandle handle,
                                                 mach_port_t* port);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_PLATFORM_HANDLE_H_
