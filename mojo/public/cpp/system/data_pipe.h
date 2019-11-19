// Copyright 2014 The Chromium Authors. All rights reserved.
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

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

// A strongly-typed representation of a |MojoHandle| to the producer end of a
// data pipe.
class DataPipeProducerHandle : public Handle {
 public:
  DataPipeProducerHandle() {}
  explicit DataPipeProducerHandle(MojoHandle value) : Handle(value) {}

  // Writes to a data pipe. See |MojoWriteData| for complete documentation.
  MojoResult WriteData(const void* elements,
                       uint32_t* num_bytes,
                       MojoWriteDataFlags flags) const {
    MojoWriteDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;
    return MojoWriteData(value(), elements, num_bytes, &options);
  }

  // Begins a two-phase write to a data pipe. See |MojoBeginWriteData()| for
  // complete documentation.
  MojoResult BeginWriteData(void** buffer,
                            uint32_t* buffer_num_bytes,
                            MojoBeginWriteDataFlags flags) const {
    MojoBeginWriteDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;
    return MojoBeginWriteData(value(), &options, buffer, buffer_num_bytes);
  }

  // Completes a two-phase write to a data pipe. See |MojoEndWriteData()| for
  // complete documentation.
  MojoResult EndWriteData(uint32_t num_bytes_written) const {
    return MojoEndWriteData(value(), num_bytes_written, nullptr);
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
  MojoResult ReadData(void* elements,
                      uint32_t* num_bytes,
                      MojoReadDataFlags flags) const {
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;
    return MojoReadData(value(), &options, elements, num_bytes);
  }

  // Begins a two-phase read from a data pipe. See |MojoBeginReadData()| for
  // complete documentation.
  MojoResult BeginReadData(const void** buffer,
                           uint32_t* buffer_num_bytes,
                           MojoBeginReadDataFlags flags) const {
    MojoBeginReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;
    return MojoBeginReadData(value(), &options, buffer, buffer_num_bytes);
  }

  // Completes a two-phase read from a data pipe. See |MojoEndReadData()| for
  // complete documentation.
  MojoResult EndReadData(uint32_t num_bytes_read) const {
    return MojoEndReadData(value(), num_bytes_read, nullptr);
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
    ScopedDataPipeProducerHandle* data_pipe_producer,
    ScopedDataPipeConsumerHandle* data_pipe_consumer) {
  DCHECK(data_pipe_producer);
  DCHECK(data_pipe_consumer);
  DataPipeProducerHandle producer_handle;
  DataPipeConsumerHandle consumer_handle;
  MojoResult rv = MojoCreateDataPipe(options,
                                     producer_handle.mutable_value(),
                                     consumer_handle.mutable_value());
  // Reset even on failure (reduces the chances that a "stale"/incorrect handle
  // will be used).
  data_pipe_producer->reset(producer_handle);
  data_pipe_consumer->reset(consumer_handle);
  return rv;
}

// DEPRECATED: use |CreateDataPipe| instead.
//
// This class is not safe to use in production code as there is no way for it to
// report failure while creating the pipe and it will CHECK in case of failures.
//
// A wrapper class that automatically creates a data pipe and owns both handles.
class MOJO_CPP_SYSTEM_EXPORT DataPipe {
 public:
  DataPipe();
  explicit DataPipe(uint32_t capacity_num_bytes);
  explicit DataPipe(const MojoCreateDataPipeOptions& options);
  ~DataPipe();

  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_H_
