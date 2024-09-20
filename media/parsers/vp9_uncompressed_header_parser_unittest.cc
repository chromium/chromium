// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/vp9_uncompressed_header_parser.h"

#include "media/parsers/vp9_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class Vp9UncompressedHeaderParserTest : public testing::Test {
 public:
  void SetupPastIndependence(Vp9FrameHeader* fhdr) {
    vp9_uncompressed_header_parser_.SetupPastIndependence(fhdr);
  }

  const Vp9FrameContext& GetVp9DefaultFrameContextForTesting() const {
    return vp9_uncompressed_header_parser_
        .GetVp9DefaultFrameContextForTesting();
  }

  Vp9UncompressedHeaderParserTest()
      : vp9_uncompressed_header_parser_((&vp9_parser_context_)) {}

 protected:
  const Vp9LoopFilterParams& GetLoopFilter() const {
    return vp9_parser_context_.loop_filter();
  }

  Vp9Parser::Context vp9_parser_context_;
  Vp9UncompressedHeaderParser vp9_uncompressed_header_parser_;
};

TEST_F(Vp9UncompressedHeaderParserTest, SetupPastIndependence) {
  Vp9FrameHeader frame_header;

  SetupPastIndependence(&frame_header);

  EXPECT_EQ(0, frame_header.ref_frame_sign_bias[VP9_FRAME_INTRA]);
  EXPECT_EQ(0, frame_header.ref_frame_sign_bias[VP9_FRAME_LAST]);
  EXPECT_EQ(0, frame_header.ref_frame_sign_bias[VP9_FRAME_GOLDEN]);
  EXPECT_EQ(0, frame_header.ref_frame_sign_bias[VP9_FRAME_ALTREF]);

  // Verify ResetLoopfilter() result
  const Vp9LoopFilterParams& lf = GetLoopFilter();
  EXPECT_TRUE(lf.delta_enabled);
  EXPECT_TRUE(lf.delta_update);
  EXPECT_EQ(1, lf.ref_deltas[VP9_FRAME_INTRA]);
  EXPECT_EQ(0, lf.ref_deltas[VP9_FRAME_LAST]);
  EXPECT_EQ(-1, lf.ref_deltas[VP9_FRAME_GOLDEN]);
  EXPECT_EQ(-1, lf.ref_deltas[VP9_FRAME_ALTREF]);
  EXPECT_EQ(0, lf.mode_deltas[0]);
  EXPECT_EQ(0, lf.mode_deltas[1]);

  EXPECT_TRUE(frame_header.frame_context.IsValid());

  static_assert(std::is_pod<Vp9FrameContext>::value,
                "Vp9FrameContext is not POD, rewrite the next EXPECT_TRUE");
  EXPECT_TRUE(std::memcmp(&frame_header.frame_context,
                          &GetVp9DefaultFrameContextForTesting(),
                          sizeof(GetVp9DefaultFrameContextForTesting())) == 0);
}

}  // namespace media
