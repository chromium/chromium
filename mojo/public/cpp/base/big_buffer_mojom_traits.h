// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_MOJOM_TRAITS_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/mojom/base/big_buffer.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::BigBufferSharedMemoryRegionDataView,
                 mojo_base::internal::BigBufferSharedMemoryRegion> {
  static uint32_t size(
      const mojo_base::internal::BigBufferSharedMemoryRegion& region);
  static mojo::ScopedSharedBufferHandle buffer_handle(
      mojo_base::internal::BigBufferSharedMemoryRegion& region);

  static bool Read(mojo_base::mojom::BigBufferSharedMemoryRegionDataView data,
                   mojo_base::internal::BigBufferSharedMemoryRegion* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    UnionTraits<mojo_base::mojom::BigBufferDataView, mojo_base::BigBuffer> {
  static mojo_base::mojom::BigBufferDataView::Tag GetTag(
      const mojo_base::BigBuffer& buffer);

  static base::span<const uint8_t> bytes(const mojo_base::BigBuffer& buffer);
  static mojo_base::internal::BigBufferSharedMemoryRegion& shared_memory(
      mojo_base::BigBuffer& buffer);
  static bool invalid_buffer(mojo_base::BigBuffer& buffer);

  static bool Read(mojo_base::mojom::BigBufferDataView data,
                   mojo_base::BigBuffer* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    UnionTraits<mojo_base::mojom::BigBufferDataView, mojo_base::BigBufferView> {
  static mojo_base::mojom::BigBufferDataView::Tag GetTag(
      const mojo_base::BigBufferView& view);

  static base::span<const uint8_t> bytes(const mojo_base::BigBufferView& view);
  static mojo_base::internal::BigBufferSharedMemoryRegion& shared_memory(
      mojo_base::BigBufferView& view);
  static bool invalid_buffer(mojo_base::BigBufferView& buffer);

  static bool Read(mojo_base::mojom::BigBufferDataView data,
                   mojo_base::BigBufferView* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_MOJOM_TRAITS_H_
