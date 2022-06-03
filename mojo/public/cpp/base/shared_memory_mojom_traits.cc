// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"

#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                  base::ReadOnlySharedMemoryRegion>::
    IsNull(const base::ReadOnlySharedMemoryRegion& region) {
  return !region.IsValid();
}

// static
void StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                  base::ReadOnlySharedMemoryRegion>::
    SetToNull(base::ReadOnlySharedMemoryRegion* region) {
  *region = base::ReadOnlySharedMemoryRegion();
}

// static
mojo::ScopedSharedBufferHandle StructTraits<
    mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
    base::ReadOnlySharedMemoryRegion>::buffer(base::ReadOnlySharedMemoryRegion&
                                                  in_region) {
  return mojo::WrapReadOnlySharedMemoryRegion(std::move(in_region));
}

// static
bool StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                  base::ReadOnlySharedMemoryRegion>::
    Read(mojo_base::mojom::ReadOnlySharedMemoryRegionDataView data,
         base::ReadOnlySharedMemoryRegion* out) {
  *out = mojo::UnwrapReadOnlySharedMemoryRegion(data.TakeBuffer());
  return out->IsValid();
}

// static
bool StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                  base::UnsafeSharedMemoryRegion>::
    IsNull(const base::UnsafeSharedMemoryRegion& region) {
  return !region.IsValid();
}

// static
void StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                  base::UnsafeSharedMemoryRegion>::
    SetToNull(base::UnsafeSharedMemoryRegion* region) {
  *region = base::UnsafeSharedMemoryRegion();
}

// static
mojo::ScopedSharedBufferHandle StructTraits<
    mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
    base::UnsafeSharedMemoryRegion>::buffer(base::UnsafeSharedMemoryRegion&
                                                in_region) {
  return mojo::WrapUnsafeSharedMemoryRegion(std::move(in_region));
}

// static
bool StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                  base::UnsafeSharedMemoryRegion>::
    Read(mojo_base::mojom::UnsafeSharedMemoryRegionDataView data,
         base::UnsafeSharedMemoryRegion* out) {
  *out = mojo::UnwrapUnsafeSharedMemoryRegion(data.TakeBuffer());
  return out->IsValid();
}

// static
bool StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                  base::WritableSharedMemoryRegion>::
    IsNull(const base::WritableSharedMemoryRegion& region) {
  return !region.IsValid();
}

// static
void StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                  base::WritableSharedMemoryRegion>::
    SetToNull(base::WritableSharedMemoryRegion* region) {
  *region = base::WritableSharedMemoryRegion();
}

// static
mojo::ScopedSharedBufferHandle StructTraits<
    mojo_base::mojom::WritableSharedMemoryRegionDataView,
    base::WritableSharedMemoryRegion>::buffer(base::WritableSharedMemoryRegion&
                                                  in_region) {
  return mojo::WrapWritableSharedMemoryRegion(std::move(in_region));
}

// static
bool StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                  base::WritableSharedMemoryRegion>::
    Read(mojo_base::mojom::WritableSharedMemoryRegionDataView data,
         base::WritableSharedMemoryRegion* out) {
  *out = mojo::UnwrapWritableSharedMemoryRegion(data.TakeBuffer());
  return out->IsValid();
}

}  // namespace mojo
