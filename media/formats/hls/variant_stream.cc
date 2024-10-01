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
    std::optional<types::DecimalInteger> average_bandwidth,
    std::optional<types::DecimalFloatingPoint> score,
    std::optional<std::vector<std::string>> codecs,
    std::optional<types::DecimalResolution> resolution,
    std::optional<types::DecimalFloatingPoint> frame_rate,
    scoped_refptr<AudioRenditionGroup> audio_rendition_group,
    std::optional<std::string> video_rendition_group_name)
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

const std::string VariantStream::Format(
    const std::vector<FormatComponent>& components,
    uint32_t stream_index) const {
  std::stringstream format;
  for (FormatComponent component : components) {
    if (format.tellp()) {
      format << " ";
    }
    switch (component) {
      case FormatComponent::kResolution: {
        if (!resolution_) {
          continue;
        }
        format << resolution_->width << "x" << resolution_->height;
        break;
      }
      case FormatComponent::kFrameRate: {
        if (!frame_rate_) {
          continue;
        }
        format << frame_rate_.value() << "fps";
        break;
      }
      case FormatComponent::kCodecs: {
        if (!codecs_) {
          continue;
        }
        std::string sep = "";
        for (const std::string& codec : codecs_.value()) {
          // Get a human readable string for the codec.
          format << codec << sep;
          sep = ", ";
        }
        break;
      }
      case FormatComponent::kScore: {
        if (!score_) {
          continue;
        }
        format << "*" << score_.value();
        break;
      }
      case FormatComponent::kBandwidth: {
        if (bandwidth_ > 1000000) {
          format << bandwidth_ / 1000000 << " Mbps";
        } else {
          format << bandwidth_ / 1000 << " Kbps";
        }
        break;
      }
      case FormatComponent::kUri: {
        format << primary_rendition_uri_.ExtractFileName();
        break;
      }
      case FormatComponent::kIndex: {
        format << "Stream: " << stream_index;
        break;
      }
    }
  }
  return format.str();
}

}  // namespace media::hls
