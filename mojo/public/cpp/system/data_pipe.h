// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a C++ wrapping around the Mojo C API for data pipes,
// replacing the prefix of "Mojo" with a "mojo" namespace, and using more
// strongly-typed representations of |MojoHandle|s.
//
// Please see "mojo/public/c/system/data_pipe.h" for complete documentation of
// the API.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

// A strongly-typed representation of a |MojoHandle| to the producer end of a
// data pipe.
class DataPipeProducerHandle : public Handle {
 public:
  DataPipeProducerHandle() {}
  explicit DataPipeProducerHandle(MojoHandle value) : Handle(value) {}

  // Writes to a data pipe. See |MojoWriteData| for complete documentation.
  //
  // `data` points to the buffer with the data to write.  `data` bigger than
  // 2^32 bytes may result in `MOJO_RESULT_INVALID_ARGUMENT` unless
  // `MojoCreateDataPipeOptions::element_num_bytes` is 1.
  //
  // `bytes_written` is an out parameter that on `MOJO_RESULT_OK` communicates
  // how many bytes (from the beginning of `data`) were actually written into
  // the mojo data pipe (depending on the size of the internal pipe buffer, this
  // may be less than `data.size()`).
  //
  // Note that instead of passing specific `flags`, a more direct method can be
  // used instead:
  // - `MOJO_WRITE_DATA_FLAG_ALL_OR_NONE` => `WriteAllData`
  MojoResult WriteData(base::span<const uint8_t> data,
                       MojoWriteDataFlags flags,
                       size_t& bytes_written) const {
    MojoWriteDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;

    // Because of ABI-stability requirements, the C-level APIs take `uint32_t`.
    // But, for compatibility with C++ containers, the C++ APIs take `size_t`.
    //
    // We use `saturated_cast` so that when `num_bytes` doesn't fit into
    // `uint32_t`, then we will simply report that a smaller number of bytes was
    // written.  We accept that `num_bytes_u32` may no longer being a multiple
    // of `MojoCreateDataPipeOptions::element_num_bytes` and rely on the C layer
    // to return `MOJO_RESULT_INVALID_ARGUMENT` in this case (as reflected in
    // the doc comment above).
    uint32_t num_bytes_u32 = base::saturated_cast<uint32_t>(data.size());

    MojoResult result =
        MojoWriteData(value(), data.data(), &num_bytes_u32, &options);
    bytes_written = size_t{num_bytes_u32};
    return result;
  }

  MojoResult WriteAllData(base::span<const uint8_t> data) const {
    // Ok to ignore `bytes_written` because `MOJO_WRITE_DATA_FLAG_ALL_OR_NONE`
    // means that `MOJO_RESULT_OK` will be returned only if exactly
    // `data.size()` bytes have been written (i.e. if `data.size() ==
    // bytes_written`).
    size_t bytes_written = 0;
    return WriteData(data, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE, bytes_written);
  }

  // Using `kNoSizeHint` as `write_size_hint` argument of `BeginWriteData`
  // conveys that the caller doesn't know (or care) how much data needs to be
  // written into the buffer.
  static constexpr size_t kNoSizeHint = 0;

  // Begins a two-phase write to a data pipe. See |MojoBeginWriteData()| for
  // complete documentation.
  //
  // `buffer` is an out parameter that on `MOJO_RESULT_OK` points to a mutable,
  // pipe-owned buffer, that the caller can write into.  The caller should
  // call `EndWriteData` when they are done writing into the `buffer`.
  MojoResult BeginWriteData(size_t write_size_hint,
                            MojoBeginWriteDataFlags flags,
                            base::span<uint8_t>& buffer) const {
    MojoBeginWriteDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;

    // Because of ABI-stability requirements, the C-level APIs take `uint32_t`.
    // But, for compatibility with C++ containers, the C++ APIs take `size_t`.
    //
    // As documented by MojoBeginWriteData, on input `write_size_hint` is
    // merely a hint of how many bytes the producer is readily able to supply.
    // Therefore we use a `saturated_cast` to gracefully handle big values.
    void* buffer_ptr = nullptr;
    uint32_t buffer_num_bytes = base::saturated_cast<uint32_t>(write_size_hint);
    MojoResult result =
        MojoBeginWriteData(value(), &options, &buffer_ptr, &buffer_num_bytes);
    if (result == MOJO_RESULT_OK) {
      // SAFETY: Relying on the contract of the `MojoBeginWriteData` C API which
      // says: "On success |*buffer| will be a pointer to which the caller can
      // write up to |*buffer_num_bytes| bytes of data."
      buffer = UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(buffer_ptr),
                                         size_t{buffer_num_bytes}));
    }
    return result;
  }

  // Completes a two-phase write to a data pipe. See |MojoEndWriteData()| for
  // complete documentation.
  MojoResult EndWriteData(size_t num_bytes_written) const {
    if (!base::IsValueInRangeForNumericType<uint32_t>(num_bytes_written))
        [[unlikely]] {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    uint32_t num_bytes_written_u32 =
        base::checked_cast<uint32_t>(num_bytes_written);

    return MojoEndWriteData(value(), num_bytes_written_u32, nullptr);
  }

  // Copying and assignment allowed.
};

static_assert(sizeof(DataPipeProducerHandle) == sizeof(Handle),
              "Bad size for C++ DataPipeProducerHandle");

typedef ScopedHandleBase<DataPipeProducerHandle> ScopedDataPipeProducerHandle;
static_assert(sizeof(ScopedDataPipeProducerHandle) ==
                  sizeof(DataPipeProducerHandle),
              "Bad size for C++ ScopedDataPipeProducerHandle");

// A strongly-typed representation of a |MojoHandle| to the consumer end of a
// data pipe.
class DataPipeConsumerHandle : public Handle {
 public:
  DataPipeConsumerHandle() {}
  explicit DataPipeConsumerHandle(MojoHandle value) : Handle(value) {}

  // Reads from a data pipe. See |MojoReadData()| for complete documentation.
  //
  // `buffer` points to the buffer where `ReadData` should copy the read bytes.
  //
  // `bytes_read` is an out parameter that on `MOJO_RESULT_OK` communicates how
  // many bytes (at the beginning of `buffer`) were actually read (depending on
  // the size of the internal pipe buffer, this may be less than
  // `buffer.size()`).
  //
  // Note that instead of passing specific `flags`, a more direct method can be
  // used instead:
  // - `MOJO_READ_DATA_FLAG_DISCARD` => `DiscardData`
  MojoResult ReadData(MojoReadDataFlags flags,
                      base::span<uint8_t> buffer,
                      size_t& bytes_read) const {
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;

    // Because of ABI-stability requirements, the C-level APIs take `uint32_t`.
    // But, for compatibility with C++ containers, the C++ APIs take `size_t`.
    //
    // Input value of `*num_bytes` is ignored in `MOJO_READ_DATA_FLAG_QUERY`
    // mode and otherwise is an _upper_ bound on how many bytes will be read (or
    // discarded in `MOJO_READ_DATA_FLAG_DISCARD`).  Therefore it is okay to use
    // `saturated_cast` instead of `checked_cast` because the C-layer mojo code
    // will anyway read only up to uin32_t max bytes.
    uint32_t num_bytes = base::saturated_cast<uint32_t>(buffer.size());
    MojoResult result =
        MojoReadData(value(), &options, buffer.data(), &num_bytes);
    bytes_read = size_t{num_bytes};
    return result;
  }

  // Discards data from a data pipe. See |MojoReadData()| and
  // |MOJO_READ_DATA_FLAG_DISCARD| for complete documentation.
  //
  // `bytes_to_discard` is an input parameter that specifies how many bytes from
  // the pipe should be read and discarded.
  //
  // `bytes_discarded` is an output parameter that on `MOJO_RESULT_OK` specifies
  // how many bytes were actually discarded.
  MojoResult DiscardData(size_t bytes_to_discard,
                         size_t& bytes_discarded) const {
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_READ_DATA_FLAG_DISCARD;

    // Because of ABI-stability requirements, the C-level APIs take `uint32_t`.
    // But, for compatibility with C++ containers, the C++ APIs take `size_t`.
    //
    // Since the underlying C-API can always discard less bytes than requested,
    // we can use `saturated_cast` below.
    uint32_t num_bytes = base::saturated_cast<uint32_t>(bytes_to_discard);
    MojoResult result = MojoReadData(value(), &options, nullptr, &num_bytes);
    bytes_discarded = size_t{num_bytes};
    return result;
  }

  // Begins a two-phase read from a data pipe. See |MojoBeginReadData()| for
  // complete documentation.
  //
  // `buffer` is an out parameter that on `MOJO_RESULT_OK` points to a
  // read-only, pipe-owned buffer, that the caller can read from.  The caller
  // should call `EndReadData` when they are done reading from the `buffer`.
  MojoResult BeginReadData(MojoBeginReadDataFlags flags,
                           base::span<const uint8_t>& buffer) const {
    MojoBeginReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;

    const void* data = nullptr;
    uint32_t buffer_num_bytes = 0;
    MojoResult result =
        MojoBeginReadData(value(), &options, &data, &buffer_num_bytes);
    if (result == MOJO_RESULT_OK) {
      // SAFETY: Relying on the contract of the `MojoBeginReadData` C API which
      // says: "On success, |*buffer| will be a pointer from which the caller
      // can read up to |*buffer_num_bytes| bytes of data."
      buffer = UNSAFE_BUFFERS(base::span(static_cast<const uint8_t*>(data),
                                         size_t{buffer_num_bytes}));
    }
    return result;
  }

  // Completes a two-phase read from a data pipe. See |MojoEndReadData()| for
  // complete documentation.
  MojoResult EndReadData(size_t num_bytes_read) const {
    if (!base::IsValueInRangeForNumericType<uint32_t>(num_bytes_read))
        [[unlikely]] {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    uint32_t num_bytes_read_u32 = base::checked_cast<uint32_t>(num_bytes_read);

    return MojoEndReadData(value(), num_bytes_read_u32, nullptr);
  }

  // Copying and assignment allowed.
};

static_assert(sizeof(DataPipeConsumerHandle) == sizeof(Handle),
              "Bad size for C++ DataPipeConsumerHandle");

typedef ScopedHandleBase<DataPipeConsumerHandle> ScopedDataPipeConsumerHandle;
static_assert(sizeof(ScopedDataPipeConsumerHandle) ==
                  sizeof(DataPipeConsumerHandle),
              "Bad size for C++ ScopedDataPipeConsumerHandle");

// Creates a new data pipe. See |MojoCreateDataPipe()| for complete
// documentation.
inline MojoResult CreateDataPipe(
    const MojoCreateDataPipeOptions* options,
    ScopedDataPipeProducerHandle& data_pipe_producer,
    ScopedDataPipeConsumerHandle& data_pipe_consumer) {
  DataPipeProducerHandle producer_handle;
  DataPipeConsumerHandle consumer_handle;
  MojoResult rv = MojoCreateDataPipe(options,
                                     producer_handle.mutable_value(),
                                     consumer_handle.mutable_value());
  // Reset even on failure (reduces the chances that a "stale"/incorrect handle
  // will be used).
  data_pipe_producer.reset(producer_handle);
  data_pipe_consumer.reset(consumer_handle);
  return rv;
}

// Creates a new data pipe with a specified capacity size. For setting
// additional options, see |CreateDataPipe()| above.
inline MojoResult CreateDataPipe(
    uint32_t capacity_num_bytes,
    ScopedDataPipeProducerHandle& data_pipe_producer,
    ScopedDataPipeConsumerHandle& data_pipe_consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = capacity_num_bytes;
  return CreateDataPipe(&options, data_pipe_producer, data_pipe_consumer);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_H_
