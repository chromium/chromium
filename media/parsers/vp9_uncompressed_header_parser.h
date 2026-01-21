// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_VP9_UNCOMPRESSED_HEADER_PARSER_H_
#define MEDIA_PARSERS_VP9_UNCOMPRESSED_HEADER_PARSER_H_

#include "base/memory/raw_ptr.h"
#include "media/parsers/vp9_parser.h"
#include "media/parsers/vp9_raw_bits_reader.h"

#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT Vp9UncompressedHeaderParser {
 public:
  Vp9UncompressedHeaderParser(Vp9Parser::Context* context);

  Vp9UncompressedHeaderParser(const Vp9UncompressedHeaderParser&) = delete;
  Vp9UncompressedHeaderParser& operator=(const Vp9UncompressedHeaderParser&) =
      delete;

  // Parses VP9 uncompressed header in |stream| with |frame_size| into |fhdr|.
  // Returns true if no error.
  bool Parse(const uint8_t* stream, off_t frame_size, Vp9FrameHeader* fhdr);

  const Vp9FrameContext& GetVp9DefaultFrameContextForTesting() const;

 private:
  friend class Vp9UncompressedHeaderParserTest;

  uint8_t ReadProfile();
  bool VerifySyncCode();
  bool ReadColorConfig(Vp9FrameHeader* fhdr);
  void ReadFrameSize(Vp9FrameHeader* fhdr);
  bool ReadFrameSizeFromRefs(Vp9FrameHeader* fhdr);
  void ReadRenderSize(Vp9FrameHeader* fhdr);
  Vp9InterpolationFilter ReadInterpolationFilter();
  void ResetLoopfilter();
  void SetupPastIndependence(Vp9FrameHeader* fhdr);
  void ReadLoopFilterParams();
  void ReadQuantizationParams(Vp9QuantizationParams* quants);
  int8_t ReadDeltaQ();
  uint8_t ReadProb();
  bool ReadSegmentationParams();
  bool ReadTileInfo(Vp9FrameHeader* fhdr);

  // Raw bits reader for uncompressed frame header.
  Vp9RawBitsReader reader_;

  raw_ptr<Vp9Parser::Context> context_;
};

}  // namespace media

#endif  // MEDIA_PARSERS_VP9_UNCOMPRESSED_HEADER_PARSER_H_
