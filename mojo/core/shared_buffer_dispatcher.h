// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_SHARED_BUFFER_DISPATCHER_H_
#define MOJO_CORE_SHARED_BUFFER_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/memory/platform_shared_memory_region.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/buffer.h"

namespace mojo {

namespace core {

class NodeController;
class PlatformSharedMemoryMapping;

class MOJO_SYSTEM_IMPL_EXPORT SharedBufferDispatcher final : public Dispatcher {
 public:
  // The default options to use for |MojoCreateSharedBuffer()|. (Real uses
  // should obtain this via |ValidateCreateOptions()| with a null |in_options|;
  // this is exposed directly for testing convenience.)
  static const MojoCreateSharedBufferOptions kDefaultCreateOptions;

  // Validates and/or sets default options for |MojoCreateSharedBufferOptions|.
  // If non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateSharedBufferOptions| and will be entirely overwritten on success
  // (it may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      const MojoCreateSharedBufferOptions* in_options,
      MojoCreateSharedBufferOptions* out_options);

  // Static factory method: |validated_options| must be validated (obviously).
  // On failure, |*result| will be left as-is.
  // TODO(vtl): This should probably be made to return a scoped_refptr and have
  // a MojoResult out parameter instead.
  static MojoResult Create(
      const MojoCreateSharedBufferOptions& validated_options,
      NodeController* node_controller,
      uint64_t num_bytes,
      scoped_refptr<SharedBufferDispatcher>* result);

  // Create a |SharedBufferDispatcher| from |shared_buffer|.
  static MojoResult CreateFromPlatformSharedMemoryRegion(
      base::subtle::PlatformSharedMemoryRegion region,
      scoped_refptr<SharedBufferDispatcher>* result);

  // The "opposite" of SerializeAndClose(). Called by Dispatcher::Deserialize().
  static scoped_refptr<SharedBufferDispatcher> Deserialize(
      const void* bytes,
      size_t num_bytes,
      const ports::PortName* ports,
      size_t num_ports,
      PlatformHandle* platform_handles,
      size_t num_handles);

  SharedBufferDispatcher(const SharedBufferDispatcher&) = delete;
  SharedBufferDispatcher& operator=(const SharedBufferDispatcher&) = delete;

  // Passes the underlying PlatformSharedMemoryRegion. This dispatcher must be
  // closed after calling this function.
  base::subtle::PlatformSharedMemoryRegion PassPlatformSharedMemoryRegion();

  // NOTE: This is not thread-safe. Definitely never use it outside of tests.
  base::subtle::PlatformSharedMemoryRegion& GetRegionForTesting() {
    return region_;
  }

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult DuplicateBufferHandle(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher) override;
  MojoResult MapBuffer(
      uint64_t offset,
      uint64_t num_bytes,
      std::unique_ptr<PlatformSharedMemoryMapping>* mapping) override;
  MojoResult GetBufferInfo(MojoSharedBufferInfo* info) override;
  void StartSerialize(uint32_t* num_bytes,
                      uint32_t* num_ports,
                      uint32_t* num_platform_handles) override;
  bool EndSerialize(void* destination,
                    ports::PortName* ports,
                    PlatformHandle* handles) override;
  bool BeginTransit() override;
  void CompleteTransitAndClose() override;
  void CancelTransit() override;

 private:
  explicit SharedBufferDispatcher(
      base::subtle::PlatformSharedMemoryRegion region);
  ~SharedBufferDispatcher() override;

  static scoped_refptr<SharedBufferDispatcher> CreateInternal(
      base::subtle::PlatformSharedMemoryRegion region);

  // Validates and/or sets default options for
  // |MojoDuplicateBufferHandleOptions|. If non-null, |in_options| must point to
  // a struct of at least |in_options->struct_size| bytes. |out_options| must
  // point to a (current) |MojoDuplicateBufferHandleOptions| and will be
  // entirely overwritten on success (it may be partly overwritten on failure).
  static MojoResult ValidateDuplicateOptions(
      const MojoDuplicateBufferHandleOptions* in_options,
      MojoDuplicateBufferHandleOptions* out_options);

  // Guards access to the fields below.
  base::Lock lock_;

  bool in_transit_ = false;
  base::subtle::PlatformSharedMemoryRegion region_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_SHARED_BUFFER_DISPATCHER_H_
