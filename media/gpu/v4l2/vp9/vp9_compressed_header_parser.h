// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_VP9_VP9_COMPRESSED_HEADER_PARSER_H_
#define MEDIA_GPU_V4L2_VP9_VP9_COMPRESSED_HEADER_PARSER_H_

#include "base/memory/raw_ptr.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/vp9/vp9_bool_decoder.h"
#include "media/gpu/v4l2/vp9/vp9_v4l2_compressed_header_types.h"
#include "media/parsers/vp9_parser.h"

namespace media {

class MEDIA_GPU_EXPORT Vp9CompressedHeaderParser {
 public:
  Vp9CompressedHeaderParser();

  Vp9CompressedHeaderParser(const Vp9CompressedHeaderParser&) = delete;
  Vp9CompressedHeaderParser& operator=(const Vp9CompressedHeaderParser&) =
      delete;

  // Parses VP9 compressed header in |stream| with |frame_size| into |result|.
  // Reads uncompressed-derived fields from |frame_hdr|. Will store the
  // deltaProb in |result->frame_context| instead of updating existing
  // probabilities (see 6.3.3 Diff update prob syntax). Returns true if no
  // error.
  bool ParseNoContext(const uint8_t* stream,
                      off_t frame_size,
                      const Vp9FrameHeader& frame_hdr,
                      Vp9V4L2CompressedParseResult* result);

 private:
  bool ParseInternal(const uint8_t* stream,
                     off_t frame_size,
                     Vp9FrameHeader* fhdr);

  void ReadTxMode(Vp9FrameHeader* fhdr);
  uint8_t DecodeTermSubexp();
  void DiffUpdateProb(Vp9Prob* prob);
  template <int N>
  void DiffUpdateProbArray(Vp9Prob (&prob_array)[N]);
  void ReadTxModeProbs(Vp9FrameContext* frame_context);
  void ReadCoefProbs(Vp9FrameHeader* fhdr);
  void ReadSkipProb(Vp9FrameContext* frame_context);
  void ReadInterModeProbs(Vp9FrameContext* frame_context);
  void ReadInterpFilterProbs(Vp9FrameContext* frame_context);
  void ReadIsInterProbs(Vp9FrameContext* frame_context);
  void ReadFrameReferenceMode(Vp9FrameHeader* fhdr);
  void ReadFrameReferenceModeProbs(Vp9FrameHeader* fhdr);
  void ReadYModeProbs(Vp9FrameContext* frame_context);
  void ReadPartitionProbs(Vp9FrameContext* frame_context);
  void ReadMvProbs(bool allow_high_precision_mv,
                   Vp9FrameContext* frame_context);
  void UpdateMvProb(Vp9Prob* prob);
  template <int N>
  void UpdateMvProbArray(Vp9Prob (&prob_array)[N]);

  // Bool decoder for compressed frame header.
  Vp9BoolDecoder reader_;
  Vp9CompressedHeader::Vp9TxMode tx_mode_ = Vp9CompressedHeader::ONLY_4X4;
  Vp9ReferenceMode reference_mode_ = SINGLE_REFERENCE;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_VP9_VP9_COMPRESSED_HEADER_PARSER_H_
