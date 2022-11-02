// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_writer.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/sys_byteorder.h"
#include "third_party/blink/public/common/features.h"
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
  if (wtf_size_t num_exposed = requires_exposed_interfaces_.size();
      num_exposed && base::FeatureList::IsEnabled(
                         features::kSSVTrailerWriteExposureAssertion)) {
    uint32_t num_exposed_enc = base::HostToNet32(num_exposed);
    wtf_size_t start = trailer.size();
    trailer.Grow(start + 1 + sizeof(uint32_t) + num_exposed);
    trailer[start] = kTrailerRequiresInterfacesTag;
    memcpy(&trailer[start + 1], &num_exposed_enc, sizeof(uint32_t));
    base::ranges::copy(requires_exposed_interfaces_,
                       &trailer[start + 1 + sizeof(uint32_t)]);
  }
  return trailer;
}

}  // namespace blink
