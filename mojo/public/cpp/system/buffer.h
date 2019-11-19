// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a C++ wrapping around the Mojo C API for shared buffers,
// replacing the prefix of "Mojo" with a "mojo" namespace, and using more
// strongly-typed representations of |MojoHandle|s.
//
// Please see "mojo/public/c/system/buffer.h" for complete documentation of the
// API.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_BUFFER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_BUFFER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {
namespace internal {

struct Unmapper {
  void operator()(void* buffer) {
    MojoResult result = MojoUnmapBuffer(buffer);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }
};

}  // namespace internal

using ScopedSharedBufferMapping = std::unique_ptr<void, internal::Unmapper>;

class SharedBufferHandle;

typedef ScopedHandleBase<SharedBufferHandle> ScopedSharedBufferHandle;

// A strongly-typed representation of a |MojoHandle| referring to a shared
// buffer.
class MOJO_CPP_SYSTEM_EXPORT SharedBufferHandle : public Handle {
 public:
  enum class AccessMode {
    READ_WRITE,
    READ_ONLY,
  };

  SharedBufferHandle() {}
  explicit SharedBufferHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.

  // Creates a new SharedBufferHandle. Returns an invalid handle on failure.
  //
  // Note for those converting legacy shared memory to the
  // base::*SharedMemoryRegion API: if SharedBufferHandle::Create is used for
  // your shared memory regions, the mojo::Create*SharedMemoryRegion methods in
  // mojo/public/cpp/base/shared_memory_utils.h should be used. These know how
  // to use a broker to create regions in unprivileged contexts in the same way
  // as this SharedBufferHandle::Create method.
  static ScopedSharedBufferHandle Create(uint64_t num_bytes);

  // Clones this shared buffer handle. If |access_mode| is READ_ONLY or this is
  // a read-only handle, the new handle will be read-only. On failure, this will
  // return an empty result.
  ScopedSharedBufferHandle Clone(AccessMode access_mode) const;

  // Maps |size| bytes of this shared buffer. On failure, this will return a
  // null mapping.
  ScopedSharedBufferMapping Map(uint64_t size) const;

  // Maps |size| bytes of this shared buffer, starting |offset| bytes into the
  // buffer. On failure, this will return a null mapping.
  ScopedSharedBufferMapping MapAtOffset(uint64_t size, uint64_t offset) const;

  // Get the size of this shared buffer.
  uint64_t GetSize() const;
};

static_assert(sizeof(SharedBufferHandle) == sizeof(Handle),
              "Bad size for C++ SharedBufferHandle");
static_assert(sizeof(ScopedSharedBufferHandle) == sizeof(SharedBufferHandle),
              "Bad size for C++ ScopedSharedBufferHandle");

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_BUFFER_H_
