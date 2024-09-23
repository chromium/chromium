// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/shared_buffer_mapping.h"

#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"

namespace mojo::core::ipcz_driver {

namespace {

// Wraps and unwraps a raw PlatformSharedMemoryRegion as some more specific base
// region type to produce an appropriate SharedMemoryMapping.
template <typename RegionType>
std::unique_ptr<base::SharedMemoryMapping> MapRegion(
    base::subtle::PlatformSharedMemoryRegion& region,
    size_t offset,
    size_t size,
    void** memory) {
  auto r = RegionType::Deserialize(std::move(region));
  typename RegionType::MappingType m = r.MapAt(offset, size);
  region = RegionType::TakeHandleForSerialization(std::move(r));
  if (!m.IsValid()) {
    return nullptr;
  }
  // TODO(crbug.com/355607629): This function should give back a span instead of
  // an unbounded pointer.
  *memory = const_cast<uint8_t*>(m.data());
  return std::make_unique<typename RegionType::MappingType>(std::move(m));
}

std::unique_ptr<base::SharedMemoryMapping> MapPlatformRegion(
    base::subtle::PlatformSharedMemoryRegion& region,
    size_t offset,
    size_t size,
    void** memory) {
  using Mode = base::subtle::PlatformSharedMemoryRegion::Mode;
  switch (region.GetMode()) {
    case Mode::kReadOnly:
      return MapRegion<base::ReadOnlySharedMemoryRegion>(region, offset, size,
                                                         memory);
    case Mode::kWritable:
      return MapRegion<base::WritableSharedMemoryRegion>(region, offset, size,
                                                         memory);
    case Mode::kUnsafe:
      return MapRegion<base::UnsafeSharedMemoryRegion>(region, offset, size,
                                                       memory);
  }
  return nullptr;
}

}  // namespace

SharedBufferMapping::SharedBufferMapping(
    std::unique_ptr<base::SharedMemoryMapping> mapping,
    void* memory)
    : mapping_(std::move(mapping)), memory_(memory) {}

SharedBufferMapping::~SharedBufferMapping() = default;

// static
scoped_refptr<SharedBufferMapping> SharedBufferMapping::Create(
    base::subtle::PlatformSharedMemoryRegion& region,
    size_t offset,
    size_t size) {
  void* memory;
  auto mapping = MapPlatformRegion(region, offset, size, &memory);
  if (!mapping) {
    return nullptr;
  }

  return base::MakeRefCounted<SharedBufferMapping>(std::move(mapping), memory);
}

// static
scoped_refptr<SharedBufferMapping> SharedBufferMapping::Create(
    base::subtle::PlatformSharedMemoryRegion& region) {
  void* memory;
  auto mapping = MapPlatformRegion(region, 0, region.GetSize(), &memory);
  if (!mapping) {
    return nullptr;
  }

  return base::MakeRefCounted<SharedBufferMapping>(std::move(mapping), memory);
}

void SharedBufferMapping::Close() {
  memory_ = nullptr;
  mapping_.reset();
}

}  // namespace mojo::core::ipcz_driver
