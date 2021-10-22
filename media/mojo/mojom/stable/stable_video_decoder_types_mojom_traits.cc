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
gfx::ColorSpace::PrimaryID
StructTraits<media::stable::mojom::ColorSpaceDataView,
             gfx::ColorSpace>::primaries(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::primaries_),
                   gfx::ColorSpace::PrimaryID>::value,
      "Unexpected type for gfx::ColorSpace::primaries_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.primaries_;
}

// static
gfx::ColorSpace::TransferID
StructTraits<media::stable::mojom::ColorSpaceDataView,
             gfx::ColorSpace>::transfer(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::transfer_),
                   gfx::ColorSpace::TransferID>::value,
      "Unexpected type for gfx::ColorSpace::transfer_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.transfer_;
}

// static
gfx::ColorSpace::MatrixID
StructTraits<media::stable::mojom::ColorSpaceDataView, gfx::ColorSpace>::matrix(
    const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::matrix_),
                   gfx::ColorSpace::MatrixID>::value,
      "Unexpected type for gfx::ColorSpace::matrix_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.matrix_;
}

// static
gfx::ColorSpace::RangeID
StructTraits<media::stable::mojom::ColorSpaceDataView, gfx::ColorSpace>::range(
    const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::range_),
                   gfx::ColorSpace::RangeID>::value,
      "Unexpected type for gfx::ColorSpace::range_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.range_;
}

// static
base::span<const float> StructTraits<
    media::stable::mojom::ColorSpaceDataView,
    gfx::ColorSpace>::custom_primary_matrix(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::custom_primary_matrix_),
                   float[9]>::value,
      "Unexpected type for gfx::ColorSpace::custom_primary_matrix_. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  return input.custom_primary_matrix_;
}

// static
base::span<const float>
StructTraits<media::stable::mojom::ColorSpaceDataView,
             gfx::ColorSpace>::transfer_params(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::transfer_params_),
                   float[7]>::value,
      "Unexpected type for gfx::ColorSpace::transfer_params_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.transfer_params_;
}

// static
bool StructTraits<media::stable::mojom::ColorSpaceDataView, gfx::ColorSpace>::
    Read(media::stable::mojom::ColorSpaceDataView input,
         gfx::ColorSpace* output) {
  if (!input.ReadPrimaries(&output->primaries_))
    return false;
  if (!input.ReadTransfer(&output->transfer_))
    return false;
  if (!input.ReadMatrix(&output->matrix_))
    return false;
  if (!input.ReadRange(&output->range_))
    return false;
  {
    base::span<float> matrix(output->custom_primary_matrix_);
    if (!input.ReadCustomPrimaryMatrix(&matrix))
      return false;
  }
  {
    base::span<float> transfer(output->transfer_params_);
    if (!input.ReadTransferParams(&transfer))
      return false;
  }
  return true;
}

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
