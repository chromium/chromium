// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/shared_memory_utils.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_hooks.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

namespace {

base::WritableSharedMemoryRegion CreateWritableSharedMemoryRegion(size_t size) {
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(size);
  if (!handle.is_valid())
    return base::WritableSharedMemoryRegion();

  return mojo::UnwrapWritableSharedMemoryRegion(std::move(handle));
}

base::MappedReadOnlyRegion CreateReadOnlySharedMemoryRegion(size_t size) {
  auto writable_region = CreateWritableSharedMemoryRegion(size);
  if (!writable_region.IsValid())
    return {};

  base::WritableSharedMemoryMapping mapping = writable_region.Map();
  return {base::WritableSharedMemoryRegion::ConvertToReadOnly(
              std::move(writable_region)),
          std::move(mapping)};
}

base::UnsafeSharedMemoryRegion CreateUnsafeSharedMemoryRegion(size_t size) {
  auto writable_region = CreateWritableSharedMemoryRegion(size);
  if (!writable_region.IsValid())
    return base::UnsafeSharedMemoryRegion();

  return base::WritableSharedMemoryRegion::ConvertToUnsafe(
      std::move(writable_region));
}

}  // namespace

void SharedMemoryUtils::InstallBaseHooks() {
  base::SharedMemoryHooks::SetCreateHooks(&CreateReadOnlySharedMemoryRegion,
                                          &CreateUnsafeSharedMemoryRegion,
                                          &CreateWritableSharedMemoryRegion);
}

}  // namespace mojo
