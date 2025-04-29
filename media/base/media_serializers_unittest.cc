// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_serializers.h"

#include <memory>

#include "base/json/json_writer.h"
#include "media/base/picture_in_picture_events_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

std::string ToString(const base::Value& value) {
  if (value.is_string()) {
    return value.GetString();
  }
  std::string output_str;
  base::JSONWriter::Write(value, &output_str);
  return output_str;
}

TEST(MediaSerializersTest, BaseTypes) {
  int a = 1;
  int64_t b = 2;
  bool c = false;
  double d = 100;
  float e = 4523;
  std::string f = "foo";
  const char* g = "bar";

  ASSERT_EQ(ToString(MediaSerialize(a)), "1");
  ASSERT_EQ(ToString(MediaSerialize(b)), "0x2");
  ASSERT_EQ(ToString(MediaSerialize(c)), "false");
  ASSERT_EQ(ToString(MediaSerialize(d)), "100.0");
  ASSERT_EQ(ToString(MediaSerialize(e)), "4523.0");
  ASSERT_EQ(ToString(MediaSerialize(f)), "foo");
  ASSERT_EQ(ToString(MediaSerialize(g)), "bar");

  ASSERT_EQ(ToString(MediaSerialize("raw string")), "raw string");
}

TEST(MediaSerializersTest, Optional) {
  std::optional<int> foo;
  ASSERT_EQ(ToString(MediaSerialize(foo)), "unset");

  foo = 1;
  ASSERT_EQ(ToString(MediaSerialize(foo)), "1");
}

TEST(MediaSerializersTest, Vector) {
  std::vector<int> foo = {1, 2, 3, 6, 78, 8};
  ASSERT_EQ(ToString(MediaSerialize(foo)), "[1,2,3,6,78,8]");

  std::vector<std::string> bar = {"1", "3"};
  ASSERT_EQ(ToString(MediaSerialize(bar)), "[\"1\",\"3\"]");
}

TEST(MediaSerializersTest, AutoPipInfo) {
  using AutoPipInfo = media::PictureInPictureEventsInfo::AutoPipInfo;

  const AutoPipInfo base_info{
      .auto_pip_reason =
          media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback,
      .has_audio_focus = false,
      .is_playing = false,
      .was_recently_audible = false,
      .has_safe_url = false,
      .meets_media_engagement_conditions = false,
      .blocked_due_to_content_setting = false,
  };

  // Verify the full serialized string for the basic case.
  ASSERT_EQ(
      ToString(MediaSerialize(base_info)),
      R"({"blocked_due_to_content_setting":false,"has_audio_focus":false,)"
      R"("has_safe_url":false,"is_playing":false,)"
      R"("meets_media_engagement_conditions":false,"reason":"MediaPlayback",)"
      R"("was_recently_audible":false})");

  auto set_and_verify_flag = [&base_info](bool AutoPipInfo::* member_ptr,
                                          std::string_view expected_json) {
    AutoPipInfo modified_info = base_info;
    modified_info.*member_ptr = true;

    // Check that the expected flag is set to true in the serialized info.
    auto serialized_info = ToString(MediaSerialize(modified_info));
    EXPECT_NE(serialized_info.find(expected_json), std::string::npos)
        << "Could not find: " << expected_json;

    // Check that no other flag is set to true.
    static constexpr std::string true_string = "true";
    int counts = 0;
    std::string::size_type position = 0;
    while ((position = serialized_info.find(true_string, position)) !=
           std::string::npos) {
      counts++;
      position += true_string.length();
    }

    EXPECT_EQ(counts, 1) << "More than one flag is set to true.";
  };

  // Verify that each of the AutoPipInfo flags is properly serialized.
  set_and_verify_flag(&AutoPipInfo::has_audio_focus,
                      R"("has_audio_focus":true)");
  set_and_verify_flag(&AutoPipInfo::is_playing, R"("is_playing":true)");
  set_and_verify_flag(&AutoPipInfo::was_recently_audible,
                      R"("was_recently_audible":true)");
  set_and_verify_flag(&AutoPipInfo::has_safe_url, R"("has_safe_url":true)");
  set_and_verify_flag(&AutoPipInfo::meets_media_engagement_conditions,
                      R"("meets_media_engagement_conditions":true)");
  set_and_verify_flag(&AutoPipInfo::blocked_due_to_content_setting,
                      R"("blocked_due_to_content_setting":true)");
}

}  // namespace media
