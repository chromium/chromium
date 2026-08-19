#include <string_view>
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_NALU_UTIL_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_NALU_UTIL_H_

#include <CoreMedia/CoreMedia.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class MediaLog;

inline constexpr size_t kNALUHeaderLength = 4;

// Tracks observed (seen), active, and per-frame referenced parameter set NALUs
// (VPS, SPS, PPS).
class MEDIA_GPU_EXPORT VideoToolboxParameterSetTracker {
 public:
  VideoToolboxParameterSetTracker(std::string_view name, MediaLog* media_log);
  ~VideoToolboxParameterSetTracker();

  // Stores a parameter set NALU observed in the bitstream.
  void Process(int id, base::span<const uint8_t> nalu_data);

  // Records that a parameter set ID is referenced in the current frame.
  void ReferenceInFrame(int id);

  // Gathers the full state of all parameter sets referenced in the current
  // frame. This is used for CMFormatDescription creation. It resets the
  // internal tracking of active parameter sets, treating these as the baseline
  // for subsequent delta updates.
  bool ExtractForFormat(std::vector<const uint8_t*>& parameter_set_data_out,
                        std::vector<size_t>& parameter_set_size_out);

  // Extracts parameter sets referenced in the current frame that differ from
  // the currently active state (the state established by the most recent call
  // to ExtractForFormat or ExtractForInbandUpdate). This identifies the
  // minimal "delta" of NALUs needed for in-band configuration updates.
  // Calling this marks the extracted sets as active.
  bool ExtractForInbandUpdate(
      std::vector<base::span<const uint8_t>>& parameter_set_data_out);

  // Resets the per-frame referenced parameter set IDs.
  void ResetFrame();

  // Resets all active parameter sets (e.g. after a stream reset).
  void ResetActive();

 private:
  const std::string name_;

  // `media_log_` is owned by the object that owns this.
  const raw_ptr<MediaLog> media_log_;

  // Parameter set NALUs that have been observed in the bitstream, indexed by
  // parameter set ID.
  base::flat_map<int, std::vector<uint8_t>> seen_data_;

  // Parameter set IDs referenced by the current frame.
  base::flat_set<int> frame_ids_;

  // Parameter set NALUs currently active in the decoder, used to detect
  // changes.
  base::flat_map<int, std::vector<uint8_t>> active_data_;
};

// Takes an array of NALU spans, packs them into a CMBlockBuffer prefixed with
// 4-byte big-endian length headers, and creates a CMSampleBufferRef.
MEDIA_GPU_EXPORT base::apple::ScopedCFTypeRef<CMSampleBufferRef>
VideoToolboxCreateSampleBufferFromNALUs(
    base::span<const base::span<const uint8_t>> nalu_spans,
    CMFormatDescriptionRef format_description,
    MediaLog* media_log);

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_NALU_UTIL_H_
