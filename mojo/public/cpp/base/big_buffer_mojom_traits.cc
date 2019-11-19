// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/array_data_view.h"

namespace mojo {

// static
uint32_t StructTraits<mojo_base::mojom::BigBufferSharedMemoryRegionDataView,
                      mojo_base::internal::BigBufferSharedMemoryRegion>::
    size(const mojo_base::internal::BigBufferSharedMemoryRegion& region) {
  return base::checked_cast<uint32_t>(region.size());
}

// static
ScopedSharedBufferHandle
StructTraits<mojo_base::mojom::BigBufferSharedMemoryRegionDataView,
             mojo_base::internal::BigBufferSharedMemoryRegion>::
    buffer_handle(mojo_base::internal::BigBufferSharedMemoryRegion& region) {
  return region.TakeBufferHandle();
}

// static
bool StructTraits<mojo_base::mojom::BigBufferSharedMemoryRegionDataView,
                  mojo_base::internal::BigBufferSharedMemoryRegion>::
    Read(mojo_base::mojom::BigBufferSharedMemoryRegionDataView data,
         mojo_base::internal::BigBufferSharedMemoryRegion* out) {
  *out = mojo_base::internal::BigBufferSharedMemoryRegion(
      data.TakeBufferHandle(), data.size());
  return out->memory() != nullptr;
}

// static
mojo_base::mojom::BigBufferDataView::Tag
UnionTraits<mojo_base::mojom::BigBufferDataView, mojo_base::BigBuffer>::GetTag(
    const mojo_base::BigBuffer& buffer) {
  switch (buffer.storage_type()) {
    case mojo_base::BigBuffer::StorageType::kBytes:
      return mojo_base::mojom::BigBufferDataView::Tag::BYTES;
    case mojo_base::BigBuffer::StorageType::kSharedMemory:
      return mojo_base::mojom::BigBufferDataView::Tag::SHARED_MEMORY;
    case mojo_base::BigBuffer::StorageType::kInvalidBuffer:
      return mojo_base::mojom::BigBufferDataView::Tag::INVALID_BUFFER;
  }

  NOTREACHED();
  return mojo_base::mojom::BigBufferDataView::Tag::BYTES;
}

// static
base::span<const uint8_t>
UnionTraits<mojo_base::mojom::BigBufferDataView, mojo_base::BigBuffer>::bytes(
    const mojo_base::BigBuffer& buffer) {
  return buffer.byte_span();
}

// static
mojo_base::internal::BigBufferSharedMemoryRegion&
UnionTraits<mojo_base::mojom::BigBufferDataView,
            mojo_base::BigBuffer>::shared_memory(mojo_base::BigBuffer& buffer) {
  return buffer.shared_memory();
}

// static
bool UnionTraits<mojo_base::mojom::BigBufferDataView,
                 mojo_base::BigBuffer>::invalid_buffer(mojo_base::BigBuffer&
                                                           buffer) {
  return true;
}

// static
bool UnionTraits<mojo_base::mojom::BigBufferDataView, mojo_base::BigBuffer>::
    Read(mojo_base::mojom::BigBufferDataView data, mojo_base::BigBuffer* out) {
  switch (data.tag()) {
    case mojo_base::mojom::BigBufferDataView::Tag::BYTES: {
      mojo::ArrayDataView<uint8_t> bytes_view;
      data.GetBytesDataView(&bytes_view);
      *out = mojo_base::BigBuffer(bytes_view);
      return true;
    }

    case mojo_base::mojom::BigBufferDataView::Tag::SHARED_MEMORY: {
      mojo_base::internal::BigBufferSharedMemoryRegion shared_memory;
      if (!data.ReadSharedMemory(&shared_memory))
        return false;
      *out = mojo_base::BigBuffer(std::move(shared_memory));
      return true;
    }

    case mojo_base::mojom::BigBufferDataView::Tag::INVALID_BUFFER:
      // Always reject an invalid buffer in deserialization.
      return false;
  }

  return false;
}

// static
mojo_base::mojom::BigBufferDataView::Tag UnionTraits<
    mojo_base::mojom::BigBufferDataView,
    mojo_base::BigBufferView>::GetTag(const mojo_base::BigBufferView& view) {
  switch (view.storage_type()) {
    case mojo_base::BigBuffer::StorageType::kBytes:
      return mojo_base::mojom::BigBufferDataView::Tag::BYTES;
    case mojo_base::BigBuffer::StorageType::kSharedMemory:
      return mojo_base::mojom::BigBufferDataView::Tag::SHARED_MEMORY;
    case mojo_base::BigBuffer::StorageType::kInvalidBuffer:
      return mojo_base::mojom::BigBufferDataView::Tag::INVALID_BUFFER;
  }

  NOTREACHED();
  return mojo_base::mojom::BigBufferDataView::Tag::BYTES;
}

// static
base::span<const uint8_t> UnionTraits<
    mojo_base::mojom::BigBufferDataView,
    mojo_base::BigBufferView>::bytes(const mojo_base::BigBufferView& view) {
  return view.bytes();
}

// static
mojo_base::internal::BigBufferSharedMemoryRegion& UnionTraits<
    mojo_base::mojom::BigBufferDataView,
    mojo_base::BigBufferView>::shared_memory(mojo_base::BigBufferView& view) {
  return view.shared_memory();
}

// static
bool UnionTraits<mojo_base::mojom::BigBufferDataView,
                 mojo_base::BigBufferView>::
    invalid_buffer(mojo_base::BigBufferView& buffer) {
  return true;
}

// static
bool UnionTraits<
    mojo_base::mojom::BigBufferDataView,
    mojo_base::BigBufferView>::Read(mojo_base::mojom::BigBufferDataView data,
                                    mojo_base::BigBufferView* out) {
  switch (data.tag()) {
    case mojo_base::mojom::BigBufferDataView::Tag::BYTES: {
      mojo::ArrayDataView<uint8_t> bytes_view;
      data.GetBytesDataView(&bytes_view);
      out->SetBytes(bytes_view);
      return true;
    }

    case mojo_base::mojom::BigBufferDataView::Tag::SHARED_MEMORY: {
      mojo_base::internal::BigBufferSharedMemoryRegion shared_memory;
      if (!data.ReadSharedMemory(&shared_memory))
        return false;
      out->SetSharedMemory(std::move(shared_memory));
      return true;
    }

    case mojo_base::mojom::BigBufferDataView::Tag::INVALID_BUFFER:
      // Always reject an invalid buffer in deserialization.
      return false;
  }

  return false;
}

}  // namespace mojo
