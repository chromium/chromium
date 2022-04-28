// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/variant_stream.h"

namespace media::hls {

VariantStream::VariantStream(
    GURL primary_rendition_uri,
    types::DecimalInteger bandwidth,
    absl::optional<types::DecimalInteger> average_bandwidth,
    absl::optional<types::DecimalFloatingPoint> score,
    absl::optional<std::string> codecs,
    absl::optional<types::DecimalResolution> resolution,
    absl::optional<types::DecimalFloatingPoint> frame_rate)
    : primary_rendition_uri_(std::move(primary_rendition_uri)),
      bandwidth_(bandwidth),
      average_bandwidth_(average_bandwidth),
      score_(score),
      codecs_(std::move(codecs)),
      resolution_(resolution),
      frame_rate_(frame_rate) {}

VariantStream::VariantStream(VariantStream&&) = default;

VariantStream::~VariantStream() = default;

VariantStream& VariantStream::operator=(VariantStream&&) = default;

}  // namespace media::hls
