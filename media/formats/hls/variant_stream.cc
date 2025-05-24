// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/variant_stream.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "media/formats/hls/rendition.h"
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
    scoped_refptr<RenditionGroup> audio_rendition_group,
    scoped_refptr<RenditionGroup> video_rendition_group,
    RenditionGroup::RenditionTrack implicit_rendition)
    : primary_rendition_uri_(std::move(primary_rendition_uri)),
      bandwidth_(bandwidth),
      average_bandwidth_(average_bandwidth),
      score_(score),
      codecs_(std::move(codecs)),
      resolution_(resolution),
      frame_rate_(frame_rate),
      audio_rendition_group_(std::move(audio_rendition_group)),
      video_rendition_group_(std::move(video_rendition_group)),
      implicit_rendition_(std::move(implicit_rendition)) {}

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

void VariantStream::UpdateImplicitRenditionMediaTrackName(std::string name) {
  auto old_track = std::get<0>(implicit_rendition_);
  auto new_track = MediaTrack::CreateVideoTrack(
      /*id = */ name,
      /*kind =*/MediaTrack::VideoKind::kMain,
      /*label = */ name,
      /*language = */ "",
      /*enabled = */ old_track.enabled(),
      /*stream_id =*/old_track.stream_id());
  implicit_rendition_ =
      std::make_tuple(new_track, std::get<1>(implicit_rendition_));
}

// static
std::vector<VariantStream::FormatComponent>
VariantStream::OptimalFormatForCollection(
    const std::vector<VariantStream>& streams) {
  // We have to find some set of properties that differentiates all of the
  // supported variants. The most common differentiator is going to be
  // video resolution. Resolution is always included in the format unless some
  // of the variants are missing it or all variants have the same resolution.
  // Framerate is used as a secondary differentiator to resolution, and is only
  // used when there are two or more variants of the same resolution that have
  // differing frames rates. Bandwidth is used as a tertiary differentiator to
  // resolution and framerate.
  // If we're still in a scenario where resolution, framerate, and bandwidth are
  // all the same, we have to decide to fall back to either codecs, score, uri,
  // or index. For now just fall back to stream index, and include resolution if
  // there is more than one total size available.
  base::flat_set<types::DecimalInteger> resolutions;
  base::flat_set<types::DecimalInteger> rates;
  base::flat_set<types::DecimalInteger> bandwidths;

  bool missing_resolution = false;
  bool missing_frame_rate = false;

  for (const VariantStream& stream : streams) {
    const auto resolution = stream.GetResolution();
    const auto frame_rate = stream.GetFrameRate();
    const auto bandwidth = stream.GetBandwidth();

    if (resolution.has_value()) {
      resolutions.insert(resolution.value().Szudzik());
    } else {
      missing_resolution = true;
    }

    if (frame_rate.has_value()) {
      // FrameRate x Resolution is a bit tricky. We can't just consider one or
      // the other, because {360p, 720p} x {24fps, 60fps} would have four
      // variants, but only two resolutions or two frame rates. This isn't an
      // issue for bandwidth because it isn't an independent property like these
      // two are. To account for the fact that frame rate is only a secondary
      // differentiator, we actually hash it with resolution for a better
      // signal.
      if (frame_rate.value() > 2048) {
        // We don't support this high of a frame rate anyway! This data is
        // probably invalid, so just fall back to stream index.
        return {VariantStream::FormatComponent::kIndex};
      }
      auto resolution_and_rate = frame_rate.value();
      if (resolution.has_value()) {
        resolution_and_rate += resolution.value().Szudzik() << 11;
      }

      rates.insert(resolution_and_rate);
    } else {
      missing_frame_rate = true;
    }

    bandwidths.insert(bandwidth);
  }

  if (resolutions.size() == streams.size()) {
    // There are no duplicates of resolution, and every variant provides one.
    return {VariantStream::FormatComponent::kResolution};
  }

  if (rates.size() == streams.size()) {
    // The frame rates are a pure differentiator for variant, but we still
    // want to include resolution as well, assuming each variant has one and
    // they are not all the same.
    if (missing_resolution || resolutions.size() == 1) {
      return {VariantStream::FormatComponent::kFrameRate};
    }
    return {VariantStream::FormatComponent::kResolution,
            VariantStream::FormatComponent::kFrameRate};
  }

  if (bandwidths.size() == streams.size()) {
    if (missing_resolution || resolutions.size() == 1) {
      // Don't include resolution. Maybe frame rate?
      if (missing_frame_rate || rates.size() == 1) {
        return {VariantStream::FormatComponent::kBandwidth};
      }
      return {VariantStream::FormatComponent::kBandwidth,
              VariantStream::FormatComponent::kFrameRate};
    }
    if (missing_frame_rate || rates.size() == 1) {
      // Don't include frame rate, but resolution is ok.
      return {VariantStream::FormatComponent::kBandwidth,
              VariantStream::FormatComponent::kResolution};
    }
    return {VariantStream::FormatComponent::kBandwidth,
            VariantStream::FormatComponent::kResolution,
            VariantStream::FormatComponent::kFrameRate};
  }

  return {VariantStream::FormatComponent::kIndex};
}

}  // namespace media::hls
