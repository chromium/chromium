// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/functions and constants for platform handle wrapping
// and unwrapping APIs.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_PLATFORM_HANDLE_H_
#define MOJO_PUBLIC_C_SYSTEM_PLATFORM_HANDLE_H_

#include <stdint.h>

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The type of handle value contained in a |MojoPlatformHandle| structure.
typedef uint32_t MojoPlatformHandleType;

// An invalid handle value. Other contents of the |MojoPlatformHandle| are
// ignored.
#define MOJO_PLATFORM_HANDLE_TYPE_INVALID ((MojoPlatformHandleType)0)

// The |MojoPlatformHandle| value represents a POSIX file descriptor. Only
// usable on POSIX host systems (e.g. Android, Linux, Chrome OS, Mac).
#define MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR ((MojoPlatformHandleType)1)

// Deprecated. TYPE_MACH_PORT is equivalent to TYPE_MACH_SEND_RIGHT.
#define MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT ((MojoPlatformHandleType)2)

// The |MojoPlatformHandle| value represents a Windows HANDLE value. Only usable
// on Windows hosts.
#define MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE ((MojoPlatformHandleType)3)

// The |MojoPlatformHandle| value represents a Fuchsia system handle. Only
// usable on Fuchsia hosts.
#define MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE ((MojoPlatformHandleType)4)

// The |MojoPlatformHandle| value represents a Mach send right (e.g. a value
// opaquely of type |mach_port_t|). Only usable on macOS hosts.
#define MOJO_PLATFORM_HANDLE_TYPE_MACH_SEND_RIGHT \
  MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT

// The |MojoPlatformHandle| value represents a Mach receive right (e.g. a value
// opaquely of type |mach_port_t|). Only usable on macOS hosts.
#define MOJO_PLATFORM_HANDLE_TYPE_MACH_RECEIVE_RIGHT ((MojoPlatformHandleType)5)

// An Android IBinder reference.
#define MOJO_PLATFORM_HANDLE_TYPE_BINDER ((MojoPlatformHandleType)6)

// |MojoPlatformHandle|: A handle to a native platform object.
//
//     |uint32_t struct_size|: The size of this structure. Used for versioning
//         to allow for future extensions.
//
//     |MojoPlatformHandleType type|: The type of handle stored in |value|.
//
//     |uint64_t value|: The value of this handle. Ignored if |type| is
//         MOJO_PLATFORM_HANDLE_TYPE_INVALID. Otherwise the meaning of this
//         value depends on the value of |type|.
//

// Represents a native platform handle value for coersion to or from a wrapping
// Mojo handle.
struct MOJO_ALIGNAS(8) MojoPlatformHandle {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // The type of platform handle represented by |value|.
  MojoPlatformHandleType type;

  // An opaque representation of the native platform handle. Interpretation and
  // treatment of this value by Mojo depends on the value of |type|.
  uint64_t value;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoPlatformHandle) == 16,
                   "MojoPlatformHandle has wrong size");

// Flags passed to |MojoWrapPlatformHandle()| via
// |MojoWrapPlatformHandleOptions|.
typedef uint32_t MojoWrapPlatformHandleFlags;

// No flags. Default behavior.
#define MOJO_WRAP_PLATFORM_HANDLE_FLAG_NONE ((MojoWrapPlatformHandleFlags)0)

// Options passed to |MojoWrapPlatformHandle()|.
struct MOJO_ALIGNAS(8) MojoWrapPlatformHandleOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoWrapPlatformHandleFlags|.
  MojoWrapPlatformHandleFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoWrapPlatformHandleOptions) == 8,
                   "MojoWrapPlatformHandleOptions has wrong size");

// Flags passed to |MojoUnwrapPlatformHandle()| via
// |MojoUnwrapPlatformHandleOptions|.
typedef uint32_t MojoUnwrapPlatformHandleFlags;

// No flags. Default behavior.
#define MOJO_UNWRAP_PLATFORM_HANDLE_FLAG_NONE ((MojoUnwrapPlatformHandleFlags)0)

// Options passed to |MojoUnwrapPlatformHandle()|.
struct MOJO_ALIGNAS(8) MojoUnwrapPlatformHandleOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoUnwrapPlatformHandleFlags|.
  MojoUnwrapPlatformHandleFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoUnwrapPlatformHandleOptions) == 8,
                   "MojoUnwrapPlatformHandleOptions has wrong size");

// A GUID value used to identify the shared memory region backing a Mojo shared
// buffer handle.
struct MOJO_ALIGNAS(8) MojoSharedBufferGuid {
  uint64_t high;
  uint64_t low;
};

// The access type of shared memory region wrapped by a Mojo shared buffer
// handle. See values defined below.
typedef uint32_t MojoPlatformSharedMemoryRegionAccessMode;

// The region is read-only, meaning there is at most one writable mapped handle
// to the region somewhere, and there are any number of handles (including this
// one) which can only be mapped read-only.
//
// WARNING: See notes in |MojoWrapPlatformSharedMemoryRegion()| about the
// meaning and usage of different access modes. This CANNOT be used to change
// a buffer's access mode; it is merely an informational value to allow Mojo
// to retain consistency between wrapping and unwrapping of buffer handles.
#define MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY \
  ((MojoPlatformSharedMemoryRegionAccessMode)0)

// The region is writable, meaning there is exactly one handle to the region and
// it is mappable read/writable.
//
// WARNING: See notes in |MojoWrapPlatformSharedMemoryRegion()| about the
// meaning and usage of different access modes. This CANNOT be used to change
// a buffer's access mode; it is merely an informational value to allow Mojo
// to retain consistency between wrapping and unwrapping of buffer handles.
#define MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE \
  ((MojoPlatformSharedMemoryRegionAccessMode)1)

// The region is unsafe, meaning any number of read/writable handles may refer
// to it.
//
// WARNING: See notes in |MojoWrapPlatformSharedMemoryRegion()| about the
// meaning and usage of different access modes. This CANNOT be used to change
// a buffer's access mode; it is merely an informational value to allow Mojo
// to retain consistency between wrapping and unwrapping of buffer handles.
#define MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_UNSAFE \
  ((MojoPlatformSharedMemoryRegionAccessMode)2)

// Flags passed to |MojoWrapPlatformSharedMemoryRegion()| via
// |MojoWrapPlatformSharedMemoryRegionOptions|.
typedef uint32_t MojoWrapPlatformSharedMemoryRegionFlags;

// No flags. Default behavior.
#define MOJO_WRAP_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE \
  ((MojoWrapPlatformSharedMemoryRegionFlags)0)

// Options passed to |MojoWrapPlatformSharedMemoryRegion()|.
struct MOJO_ALIGNAS(8) MojoWrapPlatformSharedMemoryRegionOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoWrapPlatformSharedMemoryRegionFlags|.
  MojoWrapPlatformSharedMemoryRegionFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoWrapPlatformSharedMemoryRegionOptions) ==
                       8,
                   "MojoWrapPlatformSharedMemoryRegionOptions has wrong size");

// Flags passed to |MojoUnwrapPlatformSharedMemoryRegion()| via
// |MojoUnwrapPlatformSharedMemoryRegionOptions|.
typedef uint32_t MojoUnwrapPlatformSharedMemoryRegionFlags;

// No flags. Default behavior.
#define MOJO_UNWRAP_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE \
  ((MojoUnwrapPlatformSharedMemoryRegionFlags)0)

// Options passed to |MojoUnwrapPlatformSharedMemoryRegion()|.
struct MOJO_ALIGNAS(8) MojoUnwrapPlatformSharedMemoryRegionOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoUnwrapPlatformSharedMemoryRegionFlags|.
  MojoUnwrapPlatformSharedMemoryRegionFlags flags;
};
MOJO_STATIC_ASSERT(
    sizeof(struct MojoUnwrapPlatformSharedMemoryRegionOptions) == 8,
    "MojoUnwrapPlatformSharedMemoryRegionOptions has wrong size");

// Wraps a native platform handle as a Mojo handle which can be transferred
// over a message pipe. Takes ownership of the underlying native platform
// object. i.e. if you wrap a POSIX file descriptor or Windows HANDLE and then
// call |MojoClose()| on the resulting MojoHandle, the underlying file
// descriptor or HANDLE will be closed.
//
// |platform_handle|: The platform handle to wrap.
//
// |options| may be null.
//
// Returns:
//     |MOJO_RESULT_OK| if the handle was successfully wrapped. In this case
//         |*mojo_handle| contains the Mojo handle of the wrapped object.
//     |MOJO_RESULT_RESOURCE_EXHAUSTED| if the system is out of handles.
//     |MOJO_RESULT_INVALID_ARGUMENT| if |platform_handle| was not a valid
//          platform handle.
//
// NOTE: It is not always possible to detect if |platform_handle| is valid,
// particularly when |platform_handle->type| is valid but
// |platform_handle->value| does not represent a valid platform object.
MOJO_SYSTEM_EXPORT MojoResult
MojoWrapPlatformHandle(const struct MojoPlatformHandle* platform_handle,
                       const struct MojoWrapPlatformHandleOptions* options,
                       MojoHandle* mojo_handle);

// Unwraps a native platform handle from a Mojo handle. If this call succeeds,
// ownership of the underlying platform object is assumed by the caller. The
// The Mojo handle is always closed regardless of success or failure.
//
// |mojo_handle|: The Mojo handle from which to unwrap the native platform
//     handle.
//
// |options| may be null.
//
// Returns:
//     |MOJO_RESULT_OK| if the handle was successfully unwrapped. In this case
//         |*platform_handle| contains the unwrapped platform handle.
//     |MOJO_RESULT_INVALID_ARGUMENT| if |mojo_handle| was not a valid Mojo
//         handle wrapping a platform handle.
MOJO_SYSTEM_EXPORT MojoResult
MojoUnwrapPlatformHandle(MojoHandle mojo_handle,
                         const struct MojoUnwrapPlatformHandleOptions* options,
                         struct MojoPlatformHandle* platform_handle);

// Wraps a native platform shared memory region with a Mojo shared buffer handle
// which can be used exactly like a shared buffer handle created by
// |MojoCreateSharedBuffer()| or |MojoDuplicateBufferHandle()|.
//
// Takes ownership of the native platform shared buffer handle(s).
//
// |platform_handles|: The platform handle(s) to wrap. Must be one or more
//     native handles representing a shared memory region. On POSIX systems
//     with |access_mode| set to
//     |MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE| this must have
//     two handles, with the second one being a handle opened for read-only
//     mapping. For all other platforms and all other access modes, there should
//     be only one handle.
// |num_platform_handles|: The number of platform handles given in
//     |platform_handles|. See note above.
// |num_bytes|: The size of the shared memory region in bytes.
// |access_mode|: The current access mode of the shared memory region.
// |options|: Options to control behavior. May be null.
//
// !!WARNING!!: |access_mode| DOES NOT CONTROL ACCESS TO THE REGION. It is an
// informational field used by Mojo to ensure end-to-end consistency when
// wrapping and unwrapping region handles. The caller is responsible for
// ensuring that wrapped handles are already subject to the access constraints
// conveyed by |access_mode|.
//
// Returns:
//     |MOJO_RESULT_OK| if the handle was successfully wrapped. In this case
//         |*mojo_handle| contains a Mojo shared buffer handle.
//     |MOJO_RESULT_INVALID_ARGUMENT| if |platform_handle| was not a valid
//         platform shared buffer handle.
MOJO_SYSTEM_EXPORT MojoResult MojoWrapPlatformSharedMemoryRegion(
    const struct MojoPlatformHandle* platform_handles,
    uint32_t num_platform_handles,
    uint64_t num_bytes,
    const struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode access_mode,
    const struct MojoWrapPlatformSharedMemoryRegionOptions* options,
    MojoHandle* mojo_handle);

// Unwraps a native platform shared memory region from a Mojo shared buffer
// handle. If this call succeeds, ownership of the underlying shared buffer
// object is assumed by the caller.
//
// The Mojo handle is always closed regardless of success or failure.
//
// |mojo_handle|: The Mojo shared buffer handle to unwrap.
//
// On input, |*num_platform_handles| must be non-zero, and |platform_handles|
// should point to enough memory to hold at least that many |MojoPlatformHandle|
// values. Each element in |platform_handles| must have also initialized
// |struct_size| to the caller's known |sizeof(MojoPlatformHandle)|.
//
// |platform_handles|, |num_platform_handles|, |num_bytes| and |access_mode| are
// all used to receive output values and MUST always be non-null.
//
// |options| may be null.
//
// NOTE: On POSIX systems when unwrapping regions with the
// |MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE| access mode,
// this will always unwrap two platform handles, with the first one being a
// POSIX file descriptor which can be mapped to writable memory, and the second
// one being a POSIX file descriptor which can only be mapped read-only. For all
// other access modes and all other platforms, this always unwraps to a single
// platform handle.
//
// Returns:
//    |MOJO_RESULT_OK| if the handle was successfully unwrapped. In this case
//        |*platform_handles| contains one or more platform handles to represent
//        the unwrapped region, |*num_platform_handles| contains the number of
//        platform handles actually stored in |platform_handles| on output,
//        |*num_bytes| contains the size of the shared buffer object, and
//        |*access_mode| indicates the access mode of the region.
//    |MOJO_RESULT_INVALID_ARGUMENT| if |mojo_handle| is not a valid Mojo
//        shared buffer handle or |*num_platform_handles| is not large enough
//        to hold all the handles that would have been unwrapped on success.
MOJO_SYSTEM_EXPORT MojoResult MojoUnwrapPlatformSharedMemoryRegion(
    MojoHandle mojo_handle,
    const struct MojoUnwrapPlatformSharedMemoryRegionOptions* options,
    struct MojoPlatformHandle* platform_handles,
    uint32_t* num_platform_handles,
    uint64_t* num_bytes,
    struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedMemoryRegionAccessMode* access_mode);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_PLATFORM_HANDLE_H_
