// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/stable_video_decoder_types_mojom_traits.h"

// This file contains a variety of conservative compile-time assertions that
// help us detect changes that may break the backward compatibility requirement
// of the StableVideoDecoder API. Specifically, we have static_asserts() that
// ensure the type of the media struct member is *exactly* the same as the
// corresponding mojo struct member. If this changes, we must be careful to
// validate ranges and avoid implicit conversions.
//
// If you need to make any changes to this file, please consult with
// chromeos-gfx-video@google.com first.

namespace mojo {

// static
uint32_t StructTraits<
    media::stable::mojom::SubsampleEntryDataView,
    media::SubsampleEntry>::clear_bytes(const ::media::SubsampleEntry& input) {
  static_assert(
      std::is_same<
          decltype(::media::SubsampleEntry::clear_bytes),
          decltype(media::stable::mojom::SubsampleEntry::clear_bytes)>::value,
      "Unexpected type for media::SubsampleEntry::clear_bytes. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.clear_bytes;
}

// static
uint32_t StructTraits<
    media::stable::mojom::SubsampleEntryDataView,
    media::SubsampleEntry>::cypher_bytes(const ::media::SubsampleEntry& input) {
  static_assert(
      std::is_same<
          decltype(::media::SubsampleEntry::cypher_bytes),
          decltype(media::stable::mojom::SubsampleEntry::cypher_bytes)>::value,
      "Unexpected type for media::SubsampleEntry::cypher_bytes. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.cypher_bytes;
}

// static
bool StructTraits<media::stable::mojom::SubsampleEntryDataView,
                  media::SubsampleEntry>::
    Read(media::stable::mojom::SubsampleEntryDataView input,
         media::SubsampleEntry* output) {
  *output = media::SubsampleEntry(input.clear_bytes(), input.cypher_bytes());
  return true;
}

}  // namespace mojo
