// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/mojom/base/shared_memory.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                 base::ReadOnlySharedMemoryRegion> {
  static bool IsNull(const base::ReadOnlySharedMemoryRegion& region);
  static void SetToNull(base::ReadOnlySharedMemoryRegion* region);
  static mojo::ScopedSharedBufferHandle buffer(
      base::ReadOnlySharedMemoryRegion& in_region);
  static bool Read(mojo_base::mojom::ReadOnlySharedMemoryRegionDataView data,
                   base::ReadOnlySharedMemoryRegion* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                 base::UnsafeSharedMemoryRegion> {
  static bool IsNull(const base::UnsafeSharedMemoryRegion& region);
  static void SetToNull(base::UnsafeSharedMemoryRegion* region);
  static mojo::ScopedSharedBufferHandle buffer(
      base::UnsafeSharedMemoryRegion& in_region);
  static bool Read(mojo_base::mojom::UnsafeSharedMemoryRegionDataView data,
                   base::UnsafeSharedMemoryRegion* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                 base::WritableSharedMemoryRegion> {
  static bool IsNull(const base::WritableSharedMemoryRegion& region);
  static void SetToNull(base::WritableSharedMemoryRegion* region);
  static mojo::ScopedSharedBufferHandle buffer(
      base::WritableSharedMemoryRegion& in_region);
  static bool Read(mojo_base::mojom::WritableSharedMemoryRegionDataView data,
                   base::WritableSharedMemoryRegion* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_MOJOM_TRAITS_H_
