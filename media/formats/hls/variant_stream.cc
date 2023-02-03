// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/variant_stream.h"

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

VariantStream::VariantStream(
    GURL primary_rendition_uri,
    types::DecimalInteger bandwidth,
    absl::optional<types::DecimalInteger> average_bandwidth,
    absl::optional<types::DecimalFloatingPoint> score,
    absl::optional<std::vector<std::string>> codecs,
    absl::optional<types::DecimalResolution> resolution,
    absl::optional<types::DecimalFloatingPoint> frame_rate,
    scoped_refptr<AudioRenditionGroup> audio_rendition_group,
    absl::optional<std::string> video_rendition_group_name)
    : primary_rendition_uri_(std::move(primary_rendition_uri)),
      bandwidth_(bandwidth),
      average_bandwidth_(average_bandwidth),
      score_(score),
      codecs_(std::move(codecs)),
      resolution_(resolution),
      frame_rate_(frame_rate),
      audio_rendition_group_(std::move(audio_rendition_group)),
      video_rendition_group_name_(video_rendition_group_name) {}

VariantStream::VariantStream(VariantStream&&) = default;

VariantStream::~VariantStream() = default;

}  // namespace media::hls
