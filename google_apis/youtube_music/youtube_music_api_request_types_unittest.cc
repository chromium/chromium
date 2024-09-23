// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/youtube_music/youtube_music_api_request_types.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::youtube_music {

namespace {

using ::testing::Eq;

constexpr char kTestJson[] = R"(
{
  "error": {
    "code": 400,
    "message": "Invalid Request.",
    "status": "INVALID_REQUEST",
    "details": [
    {
      "@type": "type.googleapis.com/google.rpc.ErrorInfo",
      "reason": "INVALID_REQUEST",
      "domain": "googleapis.com",
      "metadata": {
        "method": "google.youtube.mediaconnect.v1.TrackService.DownloadTrack",
        "service": "youtubemediaconnect.googleapis.com",
        "mediaConnectError": "UPDATE_REQUIRED"
      }
    },
    {
      "@type": "type.googleapis.com/google.rpc.LocalizedMessage",
      "locale": "en-US",
      "message": "Upgrade to a newer version of YouTube Music."
    }
    ]
  }
})";

TEST(YoutubeMusicApiRequestTypesTest, ParseErrorJson) {
  EXPECT_THAT(ParseErrorJson(kTestJson),
              Eq("Upgrade to a newer version of YouTube Music."));
}

TEST(YoutubeMusicApiRequestTypesTest, ParseErrorJson_Empty) {
  EXPECT_THAT(ParseErrorJson(""), Eq(""));
}

TEST(YoutubeMusicApiRequestTypesTest, ParseErrorJson_EmptyDict) {
  EXPECT_THAT(ParseErrorJson("{}"), Eq(""));
}

}  // namespace

}  // namespace google_apis::youtube_music
