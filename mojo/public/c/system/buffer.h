// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/constants and functions specific to shared buffers.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_BUFFER_H_
#define MOJO_PUBLIC_C_SYSTEM_BUFFER_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Flags passed to |MojoCreateSharedBuffer()| via
// |MojoCreateSharedBufferOptions|. See values defined below.
typedef uint32_t MojoCreateSharedBufferFlags;

// No flags. Default behavior.
#define MOJO_CREATE_SHARED_BUFFER_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoCreateSharedBuffer()|.
struct MOJO_ALIGNAS(8) MojoCreateSharedBufferOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoCreateSharedBufferFlags|.
  MojoCreateSharedBufferFlags flags;
};
MOJO_STATIC_ASSERT(MOJO_ALIGNOF(int64_t) <= 8, "int64_t has weird alignment");
MOJO_STATIC_ASSERT(sizeof(struct MojoCreateSharedBufferOptions) == 8,
                   "MojoCreateSharedBufferOptions has wrong size");

// Flags passed to |MojoGetBufferInfo()| via |MojoGetBufferInfoOptions|. See
// values defined below.
typedef uint32_t MojoGetBufferInfoFlags;

// No flags. Default behavior.
#define MOJO_GET_BUFFER_INFO_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoGetBufferInfo()|.
struct MOJO_ALIGNAS(8) MojoGetBufferInfoOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoGetBufferInfoFlags|.
  MojoGetBufferInfoFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoGetBufferInfoOptions) == 8,
                   "MojoSharedBufferOptions has wrong size");

// Structure used to receive information about a shared buffer via
// |MojoGetBufferInfo()|.
struct MOJO_ALIGNAS(8) MojoSharedBufferInfo {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // The size of the shared buffer.
  uint64_t size;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoSharedBufferInfo) == 16,
                   "MojoSharedBufferInfo has wrong size");

// Flags passed to |MojoDuplicateBufferHandle()| via
// |MojoDuplicateBufferHandleOptions|. See values defined below.
typedef uint32_t MojoDuplicateBufferHandleFlags;

// No options. Default behavior. Note that if a shared buffer handle is ever
// duplicated without |MOJO_DUPLICATE_BUFFER_HANDLE_READ_ONLY| (see below),
// neither it nor any of its duplicates can ever be duplicated *with*
// |MOJO_DUPLICATE_BUFFER_HANDLE_READ_ONLY| in the future. That is, once a
// writable handle has been duplicated as another writable handle, it is no
// longer possible to create read-only handles to the underlying buffer object.
#define MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_NONE ((uint32_t)0)

// Duplicates the handle as read-only. If successful, the resulting new handle
// will always map to a read-only memory region. Successful use of this flag
// also imposes the limitation that the handle or any of its subsequent
// duplicates may never be duplicated *without* this flag in the future. That
// is, once a read-only handle is produced for a buffer object, all future
// handles to that object must also be read-only.
#define MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY ((uint32_t)1 << 0)

// Options passed to |MojoDuplicateBufferHandle()|.
struct MojoDuplicateBufferHandleOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoDuplicateBufferHandleFlags|.
  MojoDuplicateBufferHandleFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoDuplicateBufferHandleOptions) == 8,
                   "MojoDuplicateBufferHandleOptions has wrong size");

// Flags passed to |MojoMapBuffer()| via |MojoMapBufferOptions|. See values
// defined below.
typedef uint32_t MojoMapBufferFlags;

// No flags. Default behavior.
#define MOJO_MAP_BUFFER_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoMapBuffer()|.
struct MojoMapBufferOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoMapBufferFlags|.
  MojoMapBufferFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoMapBufferOptions) == 8,
                   "MojoMapBufferOptions has wrong size");

#ifdef __cplusplus
extern "C" {
#endif

// Creates a buffer of size |num_bytes| bytes that can be shared between
// processes. The returned handle may be duplicated any number of times by
// |MojoDuplicateBufferHandle()|.
//
// To access the buffer's storage, one must call |MojoMapBuffer()|.
//
// |options| may be set to null for a shared buffer with the default options.
//
// On success, |*shared_buffer_handle| will be set to the handle for the shared
// buffer. On failure it is not modified.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |*options| is invalid).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached (e.g., if the requested size was too large, or if the
//       maximum number of handles was exceeded).
//   |MOJO_RESULT_UNIMPLEMENTED| if an unsupported flag was set in |*options|.
MOJO_SYSTEM_EXPORT MojoResult
MojoCreateSharedBuffer(uint64_t num_bytes,
                       const struct MojoCreateSharedBufferOptions* options,
                       MojoHandle* shared_buffer_handle);

// Duplicates the handle |buffer_handle| as a new shared buffer handle. On
// success this returns the new handle in |*new_buffer_handle|. A shared buffer
// remains allocated as long as there is at least one shared buffer handle
// referencing it in at least one process in the system.
//
// |options| may be set to null to duplicate the buffer handle with the default
// options.
//
// Access rights to mapped memory from the duplicated handle may be controlled
// by flags in |*options|, with some limitations. See notes on
// |MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_NONE| and
// |MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY| regarding restrictions on
// duplication with respect to these flags.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |buffer_handle| is not a valid buffer handle or |*options| is invalid).
//   |MOJO_RESULT_UNIMPLEMENTED| if an unsupported flag was set in |*options|.
//   |MOJO_RESULT_FAILED_PRECONDITION| if
//       |MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY| was set but the handle
//       was already previously duplicated without that flag; or if
//       |MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY| was not set but the
//       handle was already previously duplicated with that flag.
MOJO_SYSTEM_EXPORT MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle);

// Maps the part (at offset |offset| of length |num_bytes|) of the buffer given
// by |buffer_handle| into memory, with options specified by |options|.
// |offset+num_bytes| must be less than or equal to the size of the buffer. On
// success, |*buffer| points to memory with the requested part of the buffer. On
// failure |*buffer| it is not modified.
//
// A single buffer handle may have multiple active mappings. The permissions
// (e.g., writable or executable) of the returned memory depend on the
// properties of the buffer and properties attached to the buffer handle, as
// well as |flags|.
//
// A mapped buffer must eventually be unmapped by calling |MojoUnmapBuffer()|
// with the value of |*buffer| returned by this function.
//
// |options| may be null to map the buffer with default behavior.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |buffer_handle| is not a valid buffer handle, the range specified by
//       |offset| and |num_bytes| is not valid, or |*options| is invalid).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if the mapping operation itself failed
//       (e.g., due to not having appropriate address space available).
MOJO_SYSTEM_EXPORT MojoResult
MojoMapBuffer(MojoHandle buffer_handle,
              uint64_t offset,
              uint64_t num_bytes,
              const struct MojoMapBufferOptions* options,
              void** buffer);

// Unmaps a buffer pointer that was mapped by |MojoMapBuffer()|. |buffer| must
// have been the result of |MojoMapBuffer()| (not some other pointer inside
// the mapped memory), and the entire mapping will be removed.
//
// A mapping may only be unmapped once.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |buffer| is invalid (e.g., is not the
//       result of |MojoMapBuffer()| or has already been unmapped).
MOJO_SYSTEM_EXPORT MojoResult MojoUnmapBuffer(void* buffer);

// Retrieve information about |buffer_handle| into |info|.
//
// Callers must initialize |info->struct_size| to |sizeof(MojoSharedBufferInfo)|
// before calling this function.
//
// |options| may be null for default options.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |buffer_handle| is invalid, |info| is
//       null, or |*options| is invalid.
//
// On success, |info->size| will be set to the size of the buffer. On failure it
// is not modified.
MOJO_SYSTEM_EXPORT MojoResult
MojoGetBufferInfo(MojoHandle buffer_handle,
                  const struct MojoGetBufferInfoOptions* options,
                  struct MojoSharedBufferInfo* info);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_BUFFER_H_
