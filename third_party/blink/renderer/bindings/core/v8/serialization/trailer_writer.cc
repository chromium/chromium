// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_writer.h"

#include "base/containers/span_writer.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"

namespace blink {

TrailerWriter::TrailerWriter() = default;

TrailerWriter::~TrailerWriter() = default;

void TrailerWriter::RequireExposedInterface(SerializationTag tag) {
  DCHECK_GT(tag, 0x00);
  DCHECK_LE(tag, 0xFF);
  if (!requires_exposed_interfaces_.Contains(tag))
    requires_exposed_interfaces_.push_back(tag);
}

Vector<uint8_t> TrailerWriter::MakeTrailerData() const {
  Vector<uint8_t> trailer;
  // The code below assumes that the size of SerializationTag is one byte.
  static_assert(sizeof(SerializationTag) == 1u);
  if (wtf_size_t num_exposed = requires_exposed_interfaces_.size();
      num_exposed) {
    wtf_size_t start = trailer.size();
    trailer.Grow(start + 1 + sizeof(uint32_t) + num_exposed);
    auto trailer_span = base::span(trailer);
    base::SpanWriter writer(trailer_span.subspan(start));
    writer.WriteU8BigEndian(kTrailerRequiresInterfacesTag);
    writer.WriteU32BigEndian(num_exposed);
    writer.Write(base::as_byte_span(requires_exposed_interfaces_));
    CHECK_EQ(writer.remaining(), 0u);
  }
  return trailer;
}

}  // namespace blink
