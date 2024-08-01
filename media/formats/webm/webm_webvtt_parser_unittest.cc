// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include "media/formats/webm/webm_webvtt_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;

namespace media {

typedef std::vector<uint8_t> Cue;

static Cue EncodeCue(const std::string& id,
                     const std::string& settings,
                     const std::string& content) {
  const std::string result = id + '\n' + settings + '\n' + content;
  const uint8_t* const buf = reinterpret_cast<const uint8_t*>(result.data());
  return Cue(buf, buf + result.length());
}

static void DecodeCue(const Cue& cue,
                      std::string* id,
                      std::string* settings,
                      std::string* content) {
  WebMWebVTTParser::Parse(&cue[0], static_cast<int>(cue.size()),
                          id, settings, content);
}

class WebMWebVTTParserTest : public testing::Test {
 public:
  WebMWebVTTParserTest() = default;
};

TEST_F(WebMWebVTTParserTest, Blank) {
  InSequence s;

  const Cue cue = EncodeCue("", "", "Subtitle");
  std::string id, settings, content;

  DecodeCue(cue, &id, &settings, &content);
  EXPECT_EQ(id, "");
  EXPECT_EQ(settings, "");
  EXPECT_EQ(content, "Subtitle");
}

TEST_F(WebMWebVTTParserTest, Id) {
  InSequence s;

  for (int i = 1; i <= 9; ++i) {
    const std::string idsrc(1, '0'+i);
    const Cue cue = EncodeCue(idsrc, "", "Subtitle");
    std::string id, settings, content;

    DecodeCue(cue, &id, &settings, &content);
    EXPECT_EQ(id, idsrc);
    EXPECT_EQ(settings, "");
    EXPECT_EQ(content, "Subtitle");
  }
}

TEST_F(WebMWebVTTParserTest, Settings) {
  InSequence s;

  enum { kSettingsCount = 4 };
  const char* const settings_str[kSettingsCount] = {
    "vertical:lr",
    "line:50%",
    "position:42%",
    "vertical:rl line:42% position:100%" };

  for (int i = 0; i < kSettingsCount; ++i) {
    const Cue cue = EncodeCue("", settings_str[i], "Subtitle");
    std::string id, settings, content;

    DecodeCue(cue, &id, &settings, &content);
    EXPECT_EQ(id, "");
    EXPECT_EQ(settings, settings_str[i]);
    EXPECT_EQ(content, "Subtitle");
  }
}

TEST_F(WebMWebVTTParserTest, Content) {
  InSequence s;

  enum { kContentCount = 4 };
  const char* const content_str[kContentCount] = {
    "Subtitle",
    "Another Subtitle",
    "Yet Another Subtitle",
    "Another Subtitle\nSplit Across Two Lines" };

  for (int i = 0; i < kContentCount; ++i) {
    const Cue cue = EncodeCue("", "", content_str[i]);
    std::string id, settings, content;

    DecodeCue(cue, &id, &settings, &content);
    EXPECT_EQ(id, "");
    EXPECT_EQ(settings, "");
    EXPECT_EQ(content, content_str[i]);
  }
}

}  // namespace media
