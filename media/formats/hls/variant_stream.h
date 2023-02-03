// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_VARIANT_STREAM_H_
#define MEDIA_FORMATS_HLS_VARIANT_STREAM_H_

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "media/base/media_export.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace media::hls {

class AudioRenditionGroup;

class MEDIA_EXPORT VariantStream {
 public:
  VariantStream(GURL primary_rendition_uri,
                types::DecimalInteger bandwidth,
                absl::optional<types::DecimalInteger> average_bandwidth,
                absl::optional<types::DecimalFloatingPoint> score,
                absl::optional<std::vector<std::string>> codecs,
                absl::optional<types::DecimalResolution> resolution,
                absl::optional<types::DecimalFloatingPoint> frame_rate,
                scoped_refptr<AudioRenditionGroup> audio_renditions,
                absl::optional<std::string> video_rendition_group_name);
  VariantStream(const VariantStream&) = delete;
  VariantStream(VariantStream&&);
  ~VariantStream();
  VariantStream& operator=(const VariantStream&) = delete;
  VariantStream& operator=(VariantStream&&) = delete;

  // The URI of the rendition provided by the playlist for clients that do not
  // support multiple renditions.
  const GURL& GetPrimaryRenditionUri() const { return primary_rendition_uri_; }

  // Returns the peak segment bitrate (bits/s) of this variant stream.
  //
  //  "If all the Media Segments in a Variant Stream have already been
  //  created, the BANDWIDTH value MUST be the largest sum of peak
  //  segment bit rates that is produced by any playable combination of
  //  Renditions.  (For a Variant Stream with a single Media Playlist,
  //  this is just the peak segment bit rate of that Media Playlist.)
  //  An inaccurate value can cause playback stalls or prevent clients
  //  from playing the variant.
  //
  //  If the Multivariant Playlist is to be made available before all
  //  Media Segments in the presentation have been encoded, the
  //  BANDWIDTH value SHOULD be the BANDWIDTH value of a representative
  //  period of similar content, encoded using the same settings."
  //  https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=attributes%20are%20defined%3A-,BANDWIDTH,-The%20value%20is
  types::DecimalInteger GetBandwidth() const { return bandwidth_; }

  // This represents the average segment bitrate of this variant stream. If all
  //
  //  "If all the Media Segments in a Variant Stream have already been
  //  created, the AVERAGE-BANDWIDTH value MUST be the largest sum of
  //  average segment bit rates that is produced by any playable
  //  combination of Renditions.  (For a Variant Stream with a single
  //  Media Playlist, this is just the average segment bit rate of that
  //  Media Playlist.)  An inaccurate value can cause playback stalls or
  //  prevent clients from playing the variant.
  //
  //  If the Multivariant Playlist is to be made available before all
  //  Media Segments in the presentation have been encoded, the AVERAGE-
  //  BANDWIDTH value SHOULD be the AVERAGE-BANDWIDTH value of a
  //  representative period of similar content, encoded using the same
  //  settings."
  //  https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#:~:text=the%20BANDWIDTH%20attribute.-,AVERAGE%2DBANDWIDTH,-The%20value%20is
  absl::optional<types::DecimalInteger> GetAverageBandwidth() const {
    return average_bandwidth_;
  }

  // A metric computed by the HLS server to provide a relative measure of
  // desireability for each variant. A higher score indicates that this variant
  // should be preferred over other variants with lower scores.
  absl::optional<types::DecimalFloatingPoint> GetScore() const {
    return score_;
  }

  // A list of media sample formats present in one or more renditions of this
  // variant.
  const absl::optional<std::vector<std::string>>& GetCodecs() const {
    return codecs_;
  }

  // A value representing the optimal pixel resolution at which to display all
  // video in this variant stream.
  const absl::optional<types::DecimalResolution> GetResolution() const {
    return resolution_;
  }

  // This represents the maximum framerate for all video in this variant stream.
  const absl::optional<types::DecimalFloatingPoint> GetFrameRate() const {
    return frame_rate_;
  }

  // Returns the audio rendition group that should be used when playing this
  // variant.
  const scoped_refptr<AudioRenditionGroup>& GetAudioRenditionGroup() const {
    return audio_rendition_group_;
  }

  // Returns the name of the video rendition group, if it exists.
  const absl::optional<std::string> GetVideoRenditionGroupName() const {
    return video_rendition_group_name_;
  }

 private:
  GURL primary_rendition_uri_;
  types::DecimalInteger bandwidth_;
  absl::optional<types::DecimalInteger> average_bandwidth_;
  absl::optional<types::DecimalFloatingPoint> score_;
  absl::optional<std::vector<std::string>> codecs_;
  absl::optional<types::DecimalResolution> resolution_;
  absl::optional<types::DecimalFloatingPoint> frame_rate_;
  scoped_refptr<AudioRenditionGroup> audio_rendition_group_;
  absl::optional<std::string> video_rendition_group_name_;
};

}  // namespace media::hls

#endif
