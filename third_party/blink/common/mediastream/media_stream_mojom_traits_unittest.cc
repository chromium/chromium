// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/common/mediastream/media_stream_mojom_traits.h"

#include "base/base64.h"
#include "base/rand_util.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#include <string>

namespace {
std::string GetRandomDeviceId() {
  return base::ToLowerASCII(base::HexEncode(base::RandBytesAsVector(32)));
}

std::string GetRandomOtherId() {
  // A valid UTF-8 string, but not a valid 32-byte value encoded as hex.
  static constexpr char kLetters[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "01234567890"
      "`~!@#$%^&*()_-=+[]{}\\|<>,./?'\"";

  // The generated string should be kMaxDeviceIdSize bytes long, from
  // //third_party/blink/common/mediastream/media_stream_mojom_traits.cc,
  // so that adding a letter to it makes it too long.
  std::vector<char> result(500);
  for (char& c : result) {
    c = kLetters[base::RandInt(0, sizeof(kLetters) - 1)];
  }
  return std::string(result.begin(), result.end());
}
}  // namespace

TEST(MediaStreamMojomTraitsTest,
     TrackControlsSerialization_DeviceCaptureStreamTypes) {
  for (const auto& device_stream_type :
       {blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE}) {
    blink::TrackControls input;
    input.stream_type = device_stream_type;
    input.device_ids = {
        media::AudioDeviceDescription::kDefaultDeviceId,
        media::AudioDeviceDescription::kCommunicationsDeviceId,
        GetRandomDeviceId(),
        GetRandomDeviceId(),
        GetRandomDeviceId(),
    };
    blink::TrackControls output;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
            input, output));
    EXPECT_EQ(output.stream_type, input.stream_type);
    EXPECT_EQ(output.device_ids, input.device_ids);

    // Too short
    {
      auto failing_input = input;
      failing_input.device_ids.push_back(
          base::ToLowerASCII(base::HexEncode(base::RandBytesAsVector(31))));
      EXPECT_FALSE(
          mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
              failing_input, output));
    }

    // Too long
    {
      auto failing_input = input;
      failing_input.device_ids.push_back(
          base::ToLowerASCII(base::HexEncode(base::RandBytesAsVector(33))));
      EXPECT_FALSE(
          mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
              failing_input, output));
    }

    // Invalid characters
    {
      auto failing_input = input;
      auto id =
          base::ToLowerASCII(base::HexEncode(base::RandBytesAsVector(31)));
      id += "&*";
      failing_input.device_ids.push_back(id);
      EXPECT_FALSE(
          mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
              failing_input, output));
    }

    // Uppercase
    {
      auto failing_input = input;
      failing_input.device_ids.push_back(
          base::HexEncode(base::RandBytesAsVector(32)));
      EXPECT_FALSE(
          mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
              failing_input, output));
    }

    // Too many ids
    {
      blink::TrackControls big_input;
      for (size_t i = 0; i < 100; ++i) {
        big_input.device_ids.push_back(GetRandomDeviceId());
      }
      EXPECT_TRUE(
          mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
              big_input, output));
      EXPECT_EQ(output.device_ids, big_input.device_ids);

      big_input.device_ids.push_back(GetRandomDeviceId());
      EXPECT_FALSE(
          mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
              big_input, output));
    }
  }
}

TEST(MediaStreamMojomTraitsTest, TrackControlsSerialization_OtherStreamTypes) {
  for (const auto& other_stream_type : {
           blink::mojom::MediaStreamType::NO_SERVICE,
           blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
           blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
           blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
           blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
           blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
           blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
           blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB,
           blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
       }) {
    blink::TrackControls input;
    input.stream_type = other_stream_type;
    input.device_ids = {
        GetRandomOtherId(),
        GetRandomOtherId(),
        GetRandomOtherId(),
    };
    blink::TrackControls output;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
            input, output));
    EXPECT_EQ(output.stream_type, input.stream_type);
    EXPECT_EQ(output.device_ids, input.device_ids);

    // Too long
    {
      auto failing_input = input;
      failing_input.device_ids.push_back(GetRandomOtherId() + "A");
      EXPECT_FALSE(
          mojo::test::SerializeAndDeserialize<blink::mojom::TrackControls>(
              failing_input, output));
    }
  }
}
