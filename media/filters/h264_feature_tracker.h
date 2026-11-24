// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_H264_FEATURE_TRACKER_H_
#define MEDIA_FILTERS_H264_FEATURE_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/parsers/h264_parser.h"

namespace media {

// H.264 interlacing modes reported to UMA when successfully decoded.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(H264Interlacing)
enum class H264Interlacing {
  kProgressive = 0,
  kPaff = 1,
  kMbaff = 2,
  kBoth = 3,
  kMaxValue = kBoth,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:H264Interlacing)

// Tracks H.264 content features (chroma subsampling, interlacing, profile,
// color depth) across a decoding session and reports per-session UMA metrics.
// Supports both Annex-B and AVC (avcC extradata + length-prefixed NALUs).
class MEDIA_EXPORT H264FeatureTracker {
 public:
  H264FeatureTracker();
  H264FeatureTracker(const H264FeatureTracker&) = delete;
  H264FeatureTracker& operator=(const H264FeatureTracker&) = delete;
  ~H264FeatureTracker();

  // Parses initialization data (an avcC record or Annex-B parameter sets).
  void ParseExtraData(base::span<const uint8_t> extra_data);

  // Inspects an encoded H.264 packet (AVC or Annex-B), updates cached SPS/PPS
  // state, and records content features observed in its slices.
  void ParsePacket(base::span<const uint8_t> packet_data);

  // Marks that at least one frame was successfully decoded.
  void OnFrameDecoded();

  // Emits UMA histograms once for the session if at least one frame was
  // decoded, then clears the decoded flag so subsequent calls are no-ops.
  void ReportMetrics();

 private:
  using ChromaSamplingSet = base::EnumSet<VideoChromaSampling,
                                          VideoChromaSampling::kUnknown,
                                          VideoChromaSampling::kMaxValue>;

  void ParseAvcNalus(base::span<const uint8_t> data);
  void ParseAnnexB(base::span<const uint8_t> data, bool parse_slices);
  void RecordSpsAndSlice(const H264SPS& sps, const H264SliceHeader& shdr);

  H264Parser parser_;
  size_t nal_length_size_ = 0;
  std::vector<uint8_t> nalu_scratch_;

  bool has_decoded_frame_ = false;
  bool saw_parse_error_ = false;
  bool saw_paff_ = false;
  bool saw_mbaff_ = false;
  ChromaSamplingSet chroma_samplings_;
  base::flat_set<int> color_depths_;
  base::flat_set<VideoCodecProfile> profiles_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_H264_FEATURE_TRACKER_H_
