// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_internal.h"

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

namespace {

size_t ComputeHeaderSize(uint32_t flags,
                         size_t payload_interface_id_count,
                         int64_t creation_timeticks_us) {
  if (creation_timeticks_us > 0 ||
      base::FeatureList::IsEnabled(kMojoMessageAlwaysUseLatestVersion)) {
    // Version 3
    return sizeof(MessageHeaderV3);
  } else if (payload_interface_id_count > 0) {
    // Version 2
    return sizeof(MessageHeaderV2);
  } else if (flags &
             (Message::kFlagExpectsResponse | Message::kFlagIsResponse)) {
    // Version 1
    return sizeof(MessageHeaderV1);
  } else {
    // Version 0
    return sizeof(MessageHeader);
  }
}

}  // namespace

size_t ComputeSerializedMessageSize(uint32_t flags,
                                    size_t payload_size,
                                    size_t payload_interface_id_count,
                                    int64_t creation_timeticks_us) {
  const size_t header_size = ComputeHeaderSize(
      flags, payload_interface_id_count, creation_timeticks_us);
  if (payload_interface_id_count > 0) {
    return Align(header_size + Align(payload_size) +
                 ArrayDataTraits<uint32_t>::GetStorageSize(
                     static_cast<uint32_t>(payload_interface_id_count)));
  }
  return internal::Align(header_size + payload_size);
}

size_t EstimateSerializedMessageSize(uint32_t message_name,
                                     size_t payload_size,
                                     size_t total_size,
                                     size_t estimated_payload_size) {
  if (estimated_payload_size > payload_size) {
    const size_t extra_payload_size = estimated_payload_size - payload_size;
    return internal::Align(total_size + extra_payload_size);
  }
  return total_size;
}

}  // namespace internal
}  // namespace mojo
