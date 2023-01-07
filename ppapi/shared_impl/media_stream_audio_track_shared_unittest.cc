// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/media_stream_audio_track_shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {

TEST(MediaStreamAudioTrackShared, Verify) {
  {
    MediaStreamAudioTrackShared::Attributes attributes;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));
  }

  // Verify buffers
  {
    MediaStreamAudioTrackShared::Attributes attributes;
    attributes.buffers = 0;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.buffers = 8;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.buffers = 1024;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.buffers = -1;
    EXPECT_FALSE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));
  }

  // Verify duration
  {
    MediaStreamAudioTrackShared::Attributes attributes;
    attributes.duration = 0;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.duration = 10;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.duration = 10000;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.duration = 123;
    EXPECT_TRUE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.duration = 9;
    EXPECT_FALSE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));

    attributes.duration = -1;
    EXPECT_FALSE(MediaStreamAudioTrackShared::VerifyAttributes(attributes));
  }
}

}  // namespace ppapi
