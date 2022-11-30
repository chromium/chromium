// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/constants and functions specific to data pipes.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_DATA_PIPE_H_
#define MOJO_PUBLIC_C_SYSTEM_DATA_PIPE_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Flags passed to |MojoCreateDataPipe()| via |MojoCreateDataPipeOptions|. See
// values defined below.
typedef uint32_t MojoCreateDataPipeFlags;

// No flags. Default behavior.
#define MOJO_CREATE_DATA_PIPE_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoCreateDataPipe()|.
struct MOJO_ALIGNAS(8) MojoCreateDataPipeOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoCreateDataPipeFlags|.
  MojoCreateDataPipeFlags flags;

  // The size of an element in bytes. All transactions and buffer sizes must
  // consist of an integral number of elements. Must be non-zero.
  uint32_t element_num_bytes;

  // The capacity of the data pipe in bytes. Must be a multiple of
  // |element_num_bytes|. If successfully created, the pipe will always be able
  // to queue at least this much data. If zero, the pipe buffer will be of a
  // system-dependent capacity of at least one element in size.
  uint32_t capacity_num_bytes;
};
MOJO_STATIC_ASSERT(MOJO_ALIGNOF(int64_t) <= 8, "int64_t has weird alignment");
MOJO_STATIC_ASSERT(sizeof(struct MojoCreateDataPipeOptions) == 16,
                   "MojoCreateDataPipeOptions has wrong size");

// Flags passed to |MojoWriteData()| via |MojoWriteDataOptions|. See values
// defined below.
typedef uint32_t MojoWriteDataFlags;

// No flags. Default behavior.
#define MOJO_WRITE_DATA_FLAG_NONE ((uint32_t)0)

// Requires that all provided data must fit into the pipe's available capacity
// in order for the write to succeed. Otherwise the write fails and no data is
// written into the pipe.
#define MOJO_WRITE_DATA_FLAG_ALL_OR_NONE ((uint32_t)1 << 0)

// Options passed to |MojoWriteData()|.
struct MOJO_ALIGNAS(8) MojoWriteDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoWriteDataFlags|.
  MojoWriteDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoWriteDataOptions) == 8,
                   "MojoWriteDataOptions has wrong size");

// Flags passed to |MojoBeginWriteData()| via |MojoBeginWriteDataOptions|. See
// values defined below.
typedef uint32_t MojoBeginWriteDataFlags;

// No flags. Default behavior.
#define MOJO_BEGIN_WRITE_DATA_FLAG_NONE ((uint32_t)0)

// Indicates that the size hint provided to MojoBeginWriteData() must be treated
// as a minimum requirement, and that the operation must fail if sufficient
// capacity cannot be allocated.
#define MOJO_BEGIN_WRITE_DATA_FLAG_ALL_OR_NONE ((uint32_t)1 << 0)

// Options passed to |MojoBeginWriteData()|.
struct MOJO_ALIGNAS(8) MojoBeginWriteDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoBeginWriteDataFlags|.
  MojoBeginWriteDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoBeginWriteDataOptions) == 8,
                   "MojoBeginWriteDataOptions has wrong size");

// Flags passed to |MojoEndWriteData()| via |MojoEndWriteDataOptions|. See
// values defined below.
typedef uint32_t MojoEndWriteDataFlags;

// No flags. Default behavior.
#define MOJO_END_WRITE_DATA_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoEndWriteData()|.
struct MOJO_ALIGNAS(8) MojoEndWriteDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoEndWriteDataFlags|.
  MojoEndWriteDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoEndWriteDataOptions) == 8,
                   "MojoEndWriteDataOptions has wrong size");

// Flags passed to |MojoReadData()| via |MojoReadDataOptions|.
typedef uint32_t MojoReadDataFlags;

// No flags. Default behavior.
#define MOJO_READ_DATA_FLAG_NONE ((uint32_t)0)

// Requires that all request bytes can be read from the data pipe in order for
// the read to succeed. If that many bytes are not available for reading, the
// read will fail and no bytes will be read. Ignored of
// |MOJO_READ_DATA_FLAG_QUERY| is also set.
#define MOJO_READ_DATA_FLAG_ALL_OR_NONE ((uint32_t)1 << 0)

// Discards the data read rather than copying it into the caller's provided
// buffer. May not be combined with |MOJO_READ_DATA_FLAG_PEEK| or
// |MOJO_READ_DATA_FLAG_QUERY|.
#define MOJO_READ_DATA_FLAG_DISCARD ((uint32_t)1 << 1)

// Queries the number of bytes available for reading without actually reading
// the data. May not be combined with |MOJO_READ_DATA_FLAG_DISCARD| or
// |MOJO_READ_DATA_FLAG_PEEK|. |MOJO_READ_DATA_FLAG_ALL_OR_NONE| is ignored if
// this is set.
#define MOJO_READ_DATA_FLAG_QUERY ((uint32_t)1 << 2)

// Reads data from the pipe and copies it to the caller's provided buffer
// without actually removing the data from the pipe. May not be combined with
// |MOJO_READ_DATA_FLAG_DISCARD| or |MOJO_READ_DATA_FLAG_QUERY|.
#define MOJO_READ_DATA_FLAG_PEEK ((uint32_t)1 << 3)

// Options passed to |MojoReadData()|.
struct MOJO_ALIGNAS(8) MojoReadDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoReadDataFlags|.
  MojoReadDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoReadDataOptions) == 8,
                   "MojoReadDataOptions has wrong size");

// Flags passed to |MojoBeginReadData()| via |MojoBeginReadDataOptions|. See
// values defined below.
typedef uint32_t MojoBeginReadDataFlags;

// No flags. Default behavior.
#define MOJO_BEGIN_READ_DATA_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoBeginReadData()|.
struct MOJO_ALIGNAS(8) MojoBeginReadDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoBeginReadDataFlags|.
  MojoBeginReadDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoBeginReadDataOptions) == 8,
                   "MojoBeginReadDataOptions has wrong size");

// Flags passed to |MojoEndReadData()| via |MojoEndReadDataOptions|. See
// values defined below.
typedef uint32_t MojoEndReadDataFlags;

// No flags. Default behavior.
#define MOJO_END_READ_DATA_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoEndReadData()|.
struct MOJO_ALIGNAS(8) MojoEndReadDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoEndReadDataFlags|.
  MojoEndReadDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoEndReadDataOptions) == 8,
                   "MojoEndReadDataOptions has wrong size");

#ifdef __cplusplus
extern "C" {
#endif

// Creates a data pipe, which is a unidirectional communication channel for
// unframed data. Data must be read and written in multiples of discrete
// discrete elements of size given in |options|.
//
// See |MojoCreateDataPipeOptions| for a description of the different options
// available for data pipes.
//
// |options| may be set to null for a data pipe with the default options (which
// will have an element size of one byte and have some system-dependent
// capacity).
//
// On success, |*data_pipe_producer_handle| will be set to the handle for the
// producer and |*data_pipe_consumer_handle| will be set to the handle for the
// consumer. On failure they are not modified.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid, e.g.,
//       |*options| is invalid, specified capacity or element size is zero, or
//       the specified element size exceeds the specified capacity.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached (e.g., if the requested capacity was too large, or if the
//       maximum number of handles was exceeded).
//   |MOJO_RESULT_UNIMPLEMENTED| if an unsupported flag was set in |*options|.
MOJO_SYSTEM_EXPORT MojoResult
MojoCreateDataPipe(const struct MojoCreateDataPipeOptions* options,
                   MojoHandle* data_pipe_producer_handle,
                   MojoHandle* data_pipe_consumer_handle);

// Writes the data pipe producer given by |data_pipe_producer_handle|.
//
// |elements| points to data of size |*num_bytes|; |*num_bytes| must be a
// multiple of the data pipe's element size.
//
// On success |*num_bytes| is set to the amount of data that was actually
// written. On failure it is unmodified.
//
// |options| may be null for default options. See |MojoWriteDataOptions| for
// the effect of various options on the behavior of this function.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_producer_dispatcher| is not a handle to a data pipe
//       producer or |*num_bytes| is not a multiple of the data pipe's element
//       size.)
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer handle has been
//       closed.
//   |MOJO_RESULT_OUT_OF_RANGE| if |options->flags| has
//       |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set and the required amount of data
//       (specified by |*num_bytes|) could not be written.
//   |MOJO_RESULT_BUSY| if there is a two-phase write ongoing with
//       |data_pipe_producer_handle| (i.e., |MojoBeginWriteData()| has been
//       called, but not yet the matching |MojoEndWriteData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be written (and the
//       consumer is still open) and |options->flags| does *not* have
//       |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set.
MOJO_SYSTEM_EXPORT MojoResult
MojoWriteData(MojoHandle data_pipe_producer_handle,
              const void* elements,
              uint32_t* num_bytes,
              const struct MojoWriteDataOptions* options);

// Begins a two-phase write to the data pipe producer given by
// |data_pipe_producer_handle|. On success |*buffer| will be a pointer to which
// the caller can write up to |*buffer_num_bytes| bytes of data.
//
// `*buffer_num_bytes` must be initialized on input. If non-zero it provides a
// hint about the number of data bytes the producer is readily able to supply if
// if the operation succeeds. If zero, no such hint is assumed and the value is
// ignored.
//
// If MOJO_RESULT_BEGIN_WRITE_DATA_FLAG_ALL_OR_NONE is provided in
// `options->flags`, then this hint is a minimum requirement for the operation
// to succeed, and on success the output value of `*buffer_num_bytes` will be
// at least as large as the input value.
//
// During a two-phase write, |data_pipe_producer_handle| is *not* writable.
// If another caller tries to write to it by calling |MojoWriteData()| or
// |MojoBeginWriteData()|, their request will fail with |MOJO_RESULT_BUSY|.
//
// If |MojoBeginWriteData()| returns MOJO_RESULT_OK and once the caller has
// finished writing data to |*buffer|, |MojoEndWriteData()| must be called to
// indicate the amount of data actually written and to complete the two-phase
// write operation. |MojoEndWriteData()| need not be called when
// |MojoBeginWriteData()| fails.
//
// |options| may be null for default options.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_producer_handle| is not a handle to a data pipe producer or
//       |*options| is invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer handle has been
//       closed.
//   |MOJO_RESULT_BUSY| if there is already a two-phase write ongoing with
//       |data_pipe_producer_handle| (i.e., |MojoBeginWriteData()| has been
//       called, but not yet the matching |MojoEndWriteData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be written (and the
//       consumer is still open).
MOJO_SYSTEM_EXPORT MojoResult
MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                   const struct MojoBeginWriteDataOptions* options,
                   void** buffer,
                   uint32_t* buffer_num_bytes);

// Ends a two-phase write that was previously initiated by
// |MojoBeginWriteData()| for the same |data_pipe_producer_handle|.
//
// |num_bytes_written| must indicate the number of bytes actually written into
// the two-phase write buffer. It must be less than or equal to the value of
// |*buffer_num_bytes| output by |MojoBeginWriteData()|, and it must be a
// multiple of the data pipe's element size.
//
// On failure, the two-phase write (if any) is ended (so the handle may become
// writable again if there's space available) but no data written to |*buffer|
// is "put into" the data pipe.
//
// |options| may be null for default options.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_producer_handle| is not a handle to a data pipe producer or
//       |num_bytes_written| is invalid (greater than the maximum value provided
//       by |MojoBeginWriteData()| or not a multiple of the element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer is not in a
//       two-phase write (e.g., |MojoBeginWriteData()| was not called or
//       |MojoEndWriteData()| has already been called).
MOJO_SYSTEM_EXPORT MojoResult
MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                 uint32_t num_bytes_written,
                 const struct MojoEndWriteDataOptions* options);

// Reads data from the data pipe consumer given by |data_pipe_consumer_handle|.
// May also be used to discard data or query the amount of data available.
//
// If |options->flags| has neither |MOJO_READ_DATA_FLAG_DISCARD| nor
// |MOJO_READ_DATA_FLAG_QUERY| set, this tries to read up to |*num_bytes| (which
// must be a multiple of the data pipe's element size) bytes of data to
// |elements| and set |*num_bytes| to the amount actually read. If flags has
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set, it will either read exactly
// |*num_bytes| bytes of data or none. Additionally, if flags has
// |MOJO_READ_DATA_FLAG_PEEK| set, the data read will remain in the pipe and be
// available to future reads.
//
// If flags has |MOJO_READ_DATA_FLAG_DISCARD| set, it discards up to
// |*num_bytes| (which again must be a multiple of the element size) bytes of
// data, setting |*num_bytes| to the amount actually discarded. If flags has
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE|, it will either discard exactly
// |*num_bytes| bytes of data or none. In this case, |MOJO_READ_DATA_FLAG_QUERY|
// must not be set, and |elements| is ignored (and should typically be set to
// null).
//
// If flags has |MOJO_READ_DATA_FLAG_QUERY| set, it queries the amount of data
// available, setting |*num_bytes| to the number of bytes available. In this
// case, |MOJO_READ_DATA_FLAG_DISCARD| must not be set, and
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| is ignored, as are |elements| and the input
// value of |*num_bytes|.
//
// |options| may be null for default options.
//
// Returns:
//   |MOJO_RESULT_OK| on success (see above for a description of the different
//       operations).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_consumer_handle| is invalid, the combination of flags in
//       |options->flags| is invalid, or |*options| itself is invalid).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer handle has been
//       closed and data (or the required amount of data) was not available to
//       be read or discarded.
//   |MOJO_RESULT_OUT_OF_RANGE| if |options->flags| has
//       |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set and the required amount of data
//       is not available to be read or discarded and the producer is still
//       open.
//   |MOJO_RESULT_BUSY| if there is a two-phase read ongoing with
//       |data_pipe_consumer_handle| (i.e., |MojoBeginReadData()| has been
//       called, but not yet the matching |MojoEndReadData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if there is no data to be read or discarded (and
//       the producer is still open) and |options->flags| does *not* have
//       |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set.
MOJO_SYSTEM_EXPORT MojoResult
MojoReadData(MojoHandle data_pipe_consumer_handle,
             const struct MojoReadDataOptions* options,
             void* elements,
             uint32_t* num_bytes);

// Begins a two-phase read from the data pipe consumer given by
// |data_pipe_consumer_handle|. On success, |*buffer| will be a pointer from
// which the caller can read up to |*buffer_num_bytes| bytes of data.
//
// During a two-phase read, |data_pipe_consumer_handle| is *not* readable.
// If another caller tries to read from it by calling |MojoReadData()| or
// |MojoBeginReadData()|, their request will fail with |MOJO_RESULT_BUSY|.
//
// Once the caller has finished reading data from |*buffer|, |MojoEndReadData()|
// must be called to indicate the number of bytes read and to complete the
// two-phase read operation.
//
// |options| may be null for default options.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_consumer_handle| is not a handle to a data pipe consumer,
//       or |*options| is invalid.)
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer handle has been
//       closed.
//   |MOJO_RESULT_BUSY| if there is already a two-phase read ongoing with
//       |data_pipe_consumer_handle| (i.e., |MojoBeginReadData()| has been
//       called, but not yet the matching |MojoEndReadData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be read (and the
//       producer is still open).
MOJO_SYSTEM_EXPORT MojoResult
MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                  const struct MojoBeginReadDataOptions* options,
                  const void** buffer,
                  uint32_t* buffer_num_bytes);

// Ends a two-phase read from the data pipe consumer given by
// |data_pipe_consumer_handle| that was begun by a call to |MojoBeginReadData()|
// on the same handle. |num_bytes_read| should indicate the amount of data
// actually read; it must be less than or equal to the value of
// |*buffer_num_bytes| output by |MojoBeginReadData()| and must be a multiple of
// the element size.
//
// On failure, the two-phase read (if any) is ended (so the handle may become
// readable again) but no data is "removed" from the data pipe.
//
// |options| may be null for default options.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_consumer_handle| is not a handle to a data pipe consumer or
//       |num_bytes_written| is greater than the maximum value provided by
//       |MojoBeginReadData()| or not a multiple of the element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer is not in a
//       two-phase read (e.g., |MojoBeginReadData()| was not called or
//       |MojoEndReadData()| has already been called).
MOJO_SYSTEM_EXPORT MojoResult
MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                uint32_t num_bytes_read,
                const struct MojoEndReadDataOptions* options);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_DATA_PIPE_H_
