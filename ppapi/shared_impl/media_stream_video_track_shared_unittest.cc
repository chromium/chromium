// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/shared_impl/media_stream_video_track_shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {

TEST(MediaStreamVideoTrackShared, Verify) {
  {
    MediaStreamVideoTrackShared::Attributes attributes;
    EXPECT_TRUE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));
  }

  // Verify buffers
  {
    MediaStreamVideoTrackShared::Attributes attributes;
    attributes.buffers = 0;
    EXPECT_TRUE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.buffers = 8;
    EXPECT_TRUE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.buffers = 1024;
    EXPECT_TRUE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.buffers = -1;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));
  }

  // Verify format
  {
    MediaStreamVideoTrackShared::Attributes attributes;
    for (int32_t i = PP_VIDEOFRAME_FORMAT_UNKNOWN;
         i <= PP_VIDEOFRAME_FORMAT_LAST;
         ++i) {
      attributes.format = static_cast<PP_VideoFrame_Format>(i);
      EXPECT_TRUE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));
    }

    attributes.format = static_cast<PP_VideoFrame_Format>(-1);
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.format =
        static_cast<PP_VideoFrame_Format>(PP_VIDEOFRAME_FORMAT_LAST + 1);
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));
  }

  // Verify width
  {
    MediaStreamVideoTrackShared::Attributes attributes;
    attributes.width = 1024;
    EXPECT_TRUE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.width = 1025;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.width = 1026;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.width = -1;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.width = -4;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.width = 4097;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));
  }

  // Verify height
  {
    MediaStreamVideoTrackShared::Attributes attributes;
    attributes.height = 1024;
    EXPECT_TRUE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.height = 1025;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.height = 1026;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.height = -1;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.height = -4;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

    attributes.height = 4096 + 4;
    EXPECT_FALSE(MediaStreamVideoTrackShared::VerifyAttributes(attributes));
  }

}

}  // namespace ppapi
