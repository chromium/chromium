// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/array_buffer.h"

#include <stddef.h>
#include <stdlib.h>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bits.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "gin/per_isolate_data.h"
#include "v8/include/v8-initialization.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif  // BUILDFLAG(IS_POSIX)

namespace gin {

static_assert(V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT == 2,
              "array buffers must have two internal fields");

// ArrayBufferAllocator -------------------------------------------------------
base::ThreadSafePartitionRoot* ArrayBufferAllocator::partition_ = nullptr;

void* ArrayBufferAllocator::Allocate(size_t length) {
  int flags = partition_alloc::AllocFlags::kZeroFill |
              partition_alloc::AllocFlags::kReturnNull;
  return partition_->AllocWithFlags(flags, length, "gin::ArrayBufferAllocator");
}

void* ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  int flags = partition_alloc::AllocFlags::kReturnNull;
  return partition_->AllocWithFlags(flags, length, "gin::ArrayBufferAllocator");
}

void ArrayBufferAllocator::Free(void* data, size_t length) {
  partition_->Free(data);
}

// static
ArrayBufferAllocator* ArrayBufferAllocator::SharedInstance() {
  static ArrayBufferAllocator* instance = new ArrayBufferAllocator();
  return instance;
}

// static
void ArrayBufferAllocator::InitializePartition() {
  static base::NoDestructor<base::PartitionAllocator> partition_allocator{};

  // These configuration options are copied from blink's ArrayBufferPartition.
  partition_allocator->init({
      base::PartitionOptions::AlignedAlloc::kDisallowed,
      base::PartitionOptions::ThreadCache::kDisabled,
      base::PartitionOptions::Quarantine::kAllowed,
      base::PartitionOptions::Cookie::kAllowed,
      base::PartitionOptions::BackupRefPtr::kDisabled,
      base::PartitionOptions::UseConfigurablePool::kIfAvailable,
  });

  partition_ = partition_allocator->root();
}

// ArrayBuffer ----------------------------------------------------------------
ArrayBuffer::ArrayBuffer() = default;

ArrayBuffer::ArrayBuffer(v8::Isolate* isolate, v8::Local<v8::ArrayBuffer> array)
    : backing_store_(array->GetBackingStore()) {}

ArrayBuffer::~ArrayBuffer() = default;

ArrayBuffer& ArrayBuffer::operator=(const ArrayBuffer& other) = default;

// Converter<ArrayBuffer> -----------------------------------------------------

bool Converter<ArrayBuffer>::FromV8(v8::Isolate* isolate,
                                    v8::Local<v8::Value> val,
                                    ArrayBuffer* out) {
  if (!val->IsArrayBuffer())
    return false;
  *out = ArrayBuffer(isolate, v8::Local<v8::ArrayBuffer>::Cast(val));
  return true;
}

// ArrayBufferView ------------------------------------------------------------

ArrayBufferView::ArrayBufferView()
    : offset_(0),
      num_bytes_(0) {
}

ArrayBufferView::ArrayBufferView(v8::Isolate* isolate,
                                 v8::Local<v8::ArrayBufferView> view)
    : array_buffer_(isolate, view->Buffer()),
      offset_(view->ByteOffset()),
      num_bytes_(view->ByteLength()) {
}

ArrayBufferView::~ArrayBufferView() = default;

ArrayBufferView& ArrayBufferView::operator=(const ArrayBufferView& other) =
    default;

// Converter<ArrayBufferView> -------------------------------------------------

bool Converter<ArrayBufferView>::FromV8(v8::Isolate* isolate,
                                        v8::Local<v8::Value> val,
                                        ArrayBufferView* out) {
  if (!val->IsArrayBufferView())
    return false;
  *out = ArrayBufferView(isolate, v8::Local<v8::ArrayBufferView>::Cast(val));
  return true;
}

// ArrayBufferSharedMemoryMapper ---------------------------------------------

namespace {
#ifdef V8_SANDBOX
// When the V8 sandbox is enabled, shared memory backing ArrayBuffers must be
// mapped into the sandbox address space. This custom SharedMemoryMapper
// implements this.

class ArrayBufferSharedMemoryMapper : public base::SharedMemoryMapper {
 public:
  absl::optional<base::span<uint8_t>> Map(
      base::subtle::PlatformSharedMemoryHandle handle,
      bool write_allowed,
      uint64_t offset,
      size_t size) override {
    v8::VirtualAddressSpace* address_space = v8::V8::GetSandboxAddressSpace();
    size_t allocation_granularity = address_space->allocation_granularity();

    v8::PlatformSharedMemoryHandle v8_handle;
#if BUILDFLAG(IS_MAC)
    v8_handle = v8::SharedMemoryHandleFromMachMemoryEntry(handle);
#elif BUILDFLAG(IS_FUCHSIA)
    v8_handle = v8::SharedMemoryHandleFromVMO(handle->get());
#elif BUILDFLAG(IS_WIN)
    v8_handle = v8::SharedMemoryHandleFromFileMapping(handle);
#elif BUILDFLAG(IS_ANDROID)
    v8_handle = v8::SharedMemoryHandleFromFileDescriptor(handle);
#elif BUILDFLAG(IS_POSIX)
    v8_handle = v8::SharedMemoryHandleFromFileDescriptor(handle.fd);
#else
#error "Unknown platform"
#endif

    // Size and offset must be a multiple of the page allocation granularity.
    // The caller already ensures that the offset is a multiple of the
    // allocation granularity though.
    CHECK_EQ(0UL, offset % allocation_granularity);
    size_t mapping_size = base::bits::AlignUp(size, allocation_granularity);

    v8::PagePermissions permissions = write_allowed
                                          ? v8::PagePermissions::kReadWrite
                                          : v8::PagePermissions::kRead;
    uintptr_t mapping = v8::V8::GetSandboxAddressSpace()->AllocateSharedPages(
        0, mapping_size, permissions, v8_handle, offset);
    if (!mapping)
      return absl::nullopt;

    return base::make_span(reinterpret_cast<uint8_t*>(mapping), size);
  }

  void Unmap(base::span<uint8_t> mapping) override {
    v8::VirtualAddressSpace* address_space = v8::V8::GetSandboxAddressSpace();
    size_t allocation_granularity = address_space->allocation_granularity();

    uintptr_t address = reinterpret_cast<uintptr_t>(mapping.data());
    CHECK_EQ(0UL, address % allocation_granularity);
    size_t mapping_size =
        base::bits::AlignUp(mapping.size(), allocation_granularity);

    address_space->FreeSharedPages(address, mapping_size);
  }
};
#endif  // V8_SANDBOX

base::SharedMemoryMapper* CreateSharedMemoryMapperForArrayBuffers() {
#if V8_SANDBOX
  static ArrayBufferSharedMemoryMapper instance;
  // Currently, it is still possible for the sandbox to be disabled at runtime
  // (by not initializing it), in which case the default shared memory mapper
  // must be used. In the future, this will no longer be allowed and this helper
  // function can then be removed entirely.
  // TODO(saelo) remove once sandbox initialization is mandatory.
  if (v8::V8::GetSandboxSizeInBytes() > 0)
    return &instance;
  else
#endif
    return base::SharedMemoryMapper::GetDefaultInstance();
}
}  // namespace

base::SharedMemoryMapper* GetSharedMemoryMapperForArrayBuffers() {
  static base::SharedMemoryMapper* mapper =
      CreateSharedMemoryMapperForArrayBuffers();
  return mapper;
}

}  // namespace gin
