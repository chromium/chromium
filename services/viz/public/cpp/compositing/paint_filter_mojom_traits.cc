// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/paint_filter_mojom_traits.h"

#include <utility>

#include "cc/paint/paint_filter.h"

namespace mojo {

// static
std::optional<std::vector<uint8_t>>
StructTraits<viz::mojom::PaintFilterDataView, sk_sp<cc::PaintFilter>>::data(
    const sk_sp<cc::PaintFilter>& filter) {
  std::vector<uint8_t> memory;
  memory.resize(cc::PaintOpWriter::SerializedSize(filter.get()));
  // No need to populate the SerializeOptions here since the security
  // constraints explicitly disable serializing images using the transfer cache
  // and serialization of PaintRecords.
  cc::PaintOp::SerializeOptions options;
  cc::PaintOpWriter writer(memory.data(), memory.size(), options,
                           true /* enable_security_constraints */);
  writer.Write(filter.get(), SkM44());

  if (writer.size() == 0)
    return std::nullopt;

  memory.resize(writer.size());
  return memory;
}

// static
bool StructTraits<viz::mojom::PaintFilterDataView, sk_sp<cc::PaintFilter>>::
    Read(viz::mojom::PaintFilterDataView data, sk_sp<cc::PaintFilter>* out) {
  std::optional<std::vector<uint8_t>> buffer;
  if (!data.ReadData(&buffer))
    return false;

  if (!buffer) {
    // We may fail to serialize the filter if it doesn't fit in kBufferSize
    // above, use an empty filter instead of rejecting the message.
    *out = nullptr;
    return true;
  }

  // We don't need to populate the DeserializeOptions here since the security
  // constraints explicitly disable serializing images using the transfer
  // cache/gpu::Mailbox and serialization of PaintRecords.
  std::vector<uint8_t> scratch_buffer;
  cc::PaintOp::DeserializeOptions options{.scratch_buffer = scratch_buffer};
  cc::PaintOpReader reader(buffer->data(), buffer->size(), options,
                           /*enable_security_constraints=*/true);
  sk_sp<cc::PaintFilter> filter;
  reader.Read(&filter);
  if (!reader.valid()) {
    *out = nullptr;
    return false;
  }

  // We must have consumed all bytes writen when reading this filter.
  if (reader.remaining_bytes() != 0u) {
    *out = nullptr;
    return false;
  }

  *out = std::move(filter);
  return true;
}

}  // namespace mojo
