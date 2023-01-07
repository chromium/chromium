// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_shared_memory_mapping.h"

#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "build/build_config.h"

namespace mojo {
namespace core {

PlatformSharedMemoryMapping::PlatformSharedMemoryMapping(
    base::subtle::PlatformSharedMemoryRegion* region,
    size_t offset,
    size_t length) {
  switch (region->GetMode()) {
    case base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly: {
      auto read_only_region =
          base::ReadOnlySharedMemoryRegion::Deserialize(std::move(*region));
      mapping_ = read_only_region.MapAt(offset, length);
      *region = base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(read_only_region));
      return;
    }
    case base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe: {
      auto unsafe_region =
          base::UnsafeSharedMemoryRegion::Deserialize(std::move(*region));
      mapping_ = unsafe_region.MapAt(offset, length);
      *region = base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(unsafe_region));
      return;
    }
    case base::subtle::PlatformSharedMemoryRegion::Mode::kWritable: {
      auto writable_region =
          base::WritableSharedMemoryRegion::Deserialize(std::move(*region));
      mapping_ = writable_region.MapAt(offset, length);
      *region = base::WritableSharedMemoryRegion::TakeHandleForSerialization(
          std::move(writable_region));
      return;
    }
  }
  CHECK(false);
}

PlatformSharedMemoryMapping::~PlatformSharedMemoryMapping() = default;

bool PlatformSharedMemoryMapping::IsValid() const {
  return absl::visit(
      [](const auto& member) {
        using T = std::decay_t<decltype(member)>;
        if constexpr (std::is_same_v<T, absl::monostate>) {
          return false;
        } else {
          return member.IsValid();
        }
      },
      mapping_);
}

void* PlatformSharedMemoryMapping::GetBase() const {
  return absl::visit(
      [](const auto& member) -> void* {
        using T = std::decay_t<decltype(member)>;
        if constexpr (std::is_same_v<T, absl::monostate>) {
          return nullptr;
        } else {
          return const_cast<void*>(member.memory());
        }
      },
      mapping_);
}

size_t PlatformSharedMemoryMapping::GetLength() const {
  return absl::visit(
      [](const auto& member) -> size_t {
        using T = std::decay_t<decltype(member)>;
        if constexpr (std::is_same_v<T, absl::monostate>) {
          return 0;
        } else {
          return member.size();
        }
      },
      mapping_);
}

}  // namespace core
}  // namespace mojo
