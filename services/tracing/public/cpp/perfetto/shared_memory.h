// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SHARED_MEMORY_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SHARED_MEMORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory.h"

namespace tracing {

// This wraps a Mojo SharedBuffer implementation to be
// able to provide it to Perfetto.
class COMPONENT_EXPORT(TRACING_CPP) MojoSharedMemory
    : public perfetto::SharedMemory {
 public:
  class COMPONENT_EXPORT(TRACING_CPP) Factory
      : public perfetto::SharedMemory::Factory {
   public:
    std::unique_ptr<perfetto::SharedMemory> CreateSharedMemory(
        size_t size) override;
  };

  explicit MojoSharedMemory(size_t size);
  explicit MojoSharedMemory(mojo::ScopedSharedBufferHandle shared_memory);
  ~MojoSharedMemory() override;

  // Create another wrapping instance of the same SharedMemory buffer,
  // for sending to other processes.
  mojo::ScopedSharedBufferHandle Clone();

  const mojo::ScopedSharedBufferHandle& shared_buffer() const {
    return shared_buffer_;
  }

  // perfetto::SharedMemory implementation. Called internally by Perfetto
  // classes.
  void* start() const override;
  size_t size() const override;

 private:
  mojo::ScopedSharedBufferHandle shared_buffer_;
  mojo::ScopedSharedBufferMapping mapping_;

  DISALLOW_COPY_AND_ASSIGN(MojoSharedMemory);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_SHARED_MEMORY_H_
