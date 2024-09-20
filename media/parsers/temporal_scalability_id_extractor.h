// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_TEMPORAL_SCALABILITY_ID_EXTRACTOR_H_
#define MEDIA_PARSERS_TEMPORAL_SCALABILITY_ID_EXTRACTOR_H_

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/vp9_parser.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/obu_parser.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/parsers/h265_nalu_parser.h"
#endif

namespace media {

// This class assigns a temporal layer id for each encoded frame.
// If present temporal id is taken from the encoded chunk's bitstream,
// otherwise it's assigned based on the pattern.
class MEDIA_EXPORT TemporalScalabilityIdExtractor {
 public:
  // Describe a slot of reference frame buffer.
  struct MEDIA_EXPORT ReferenceBufferSlot {
    uint32_t frame_id;
    int temporal_id;
  };
  // Metadata parsed from encoding bitstream buffer.
  struct MEDIA_EXPORT BitstreamMetadata {
    int temporal_id = 0;
    // A list of referenced frames info for this frame. Currently, only be
    // filled for VP9 encoding.
    std::vector<ReferenceBufferSlot> ref_frame_list;
    // AV1 reference_frame_idx bitmasks. Only filled for AV1 encoding. If
    // DPB slot #0 is referenced, the least significant bit is set. Similar
    // for other DPB slots.
    uint8_t reference_idx_flags;
    // AV1 refresh_frame_flags bitmasks. Only filled for AV1 encoding. The
    // least significant bit corresponds to refresh_frame_flag[0]. Similar
    // for referesh_frame_flag[1] - refresh_frame_flag[7].
    uint8_t refresh_frame_flags;
    BitstreamMetadata();
    ~BitstreamMetadata();
  };

  ~TemporalScalabilityIdExtractor();
  TemporalScalabilityIdExtractor(VideoCodec codec, int layer_count);

  // Looks at the encoded chunk and returns temporal layer id (via `md`).
  // Returns false if the bitstream is invalid or the metadata contradicts
  // the expected layering pattern.
  bool ParseChunk(base::span<const uint8_t> chunk,
                  uint32_t frame_id,
                  BitstreamMetadata& md);

  // Calculates a temporal id based on the temporal layer pattern for a given
  // number of layers.
  int AssignTemporalIdBySvcSpec(uint32_t frame_id);

 private:
  bool ParseH264(base::span<const uint8_t> chunk, BitstreamMetadata& md);
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  bool ParseHEVC(base::span<const uint8_t> chunk, BitstreamMetadata& md);
#endif

  bool ParseVP9(base::span<const uint8_t> chunk,
                uint32_t frame_id,
                int tid_by_svc_spec,
                BitstreamMetadata& md);

  bool ParseAV1(base::span<const uint8_t> chunk,
                uint32_t frame_id,
                int tid_by_svc_spec,
                BitstreamMetadata& md);

 private:
  const VideoCodec codec_;
  const int num_temporal_layers_;
  std::unique_ptr<H264Parser> h264_;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  std::unique_ptr<H265NaluParser> h265_;
#endif
  std::unique_ptr<Vp9Parser> vp9_;
  std::array<ReferenceBufferSlot, kVp9NumRefFrames> vp9_ref_buffer_;
  // These are used by AV1 only to store the state across frames.
  std::unique_ptr<libgav1::BufferPool> buffer_pool_;
  std::unique_ptr<libgav1::DecoderState> av1_decoder_state_;
  std::optional<libgav1::ObuSequenceHeader> av1_sequence_header_;
};

}  // namespace media
#endif  // MEDIA_PARSERS_TEMPORAL_SCALABILITY_ID_EXTRACTOR_H_
