// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/speech_recognition_result_mojom_traits.h"

#include <vector>

#include "base/time/time.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

base::TimeDelta kZeroTime = base::Seconds(0);
}

TEST(SpeechRecognitionResultStructTraitsTest, NoTimingInformation) {
  media::SpeechRecognitionResult result("hello world", true);
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionResult::Serialize(&result);
  media::SpeechRecognitionResult output;
  EXPECT_TRUE(media::mojom::SpeechRecognitionResult::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(result, output);
}

TEST(SpeechRecognitionResultStructTraitsTest, WithTimingInformation) {
  media::SpeechRecognitionResult invalid_result("hello world", true);
  invalid_result.timing_information = media::TimingInformation();
  invalid_result.timing_information->audio_start_time = kZeroTime;
  invalid_result.timing_information->audio_end_time = base::Seconds(-1);
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionResult::Serialize(&invalid_result);
  media::SpeechRecognitionResult output;
  EXPECT_FALSE(media::mojom::SpeechRecognitionResult::Deserialize(
      std::move(data), &output));

  media::SpeechRecognitionResult valid_result("hello world", true);
  valid_result.timing_information = media::TimingInformation();
  valid_result.timing_information->audio_start_time = kZeroTime;
  valid_result.timing_information->audio_end_time = base::Seconds(1);
  std::vector<uint8_t> valid_data =
      media::mojom::SpeechRecognitionResult::Serialize(&valid_result);
  media::SpeechRecognitionResult valid_output;
  EXPECT_TRUE(media::mojom::SpeechRecognitionResult::Deserialize(
      std::move(valid_data), &valid_output));
  EXPECT_EQ(valid_result, valid_output);
}

TEST(SpeechRecognitionResultStructTraitsTest,
     PartialResultWithTimingInformation) {
  media::SpeechRecognitionResult partial_result("hello world", false);
  partial_result.timing_information = media::TimingInformation();
  partial_result.timing_information->audio_start_time = base::Seconds(1);
  partial_result.timing_information->audio_end_time = base::Seconds(2);
  partial_result.timing_information->originating_media_timestamps =
      std::vector<media::MediaTimestampRange>();
  partial_result.timing_information->originating_media_timestamps->push_back(
      {.start = base::Seconds(10), .end = base::Seconds(11)});

  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionResult::Serialize(&partial_result);
  media::SpeechRecognitionResult output;

  EXPECT_TRUE(media::mojom::SpeechRecognitionResult::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(partial_result, output);
}

TEST(SpeechRecognitionResultStructTraitsTest, WithInvalidHypothesisParts) {
  media::SpeechRecognitionResult invalid_result("hello world", true);
  invalid_result.timing_information = media::TimingInformation();
  invalid_result.timing_information->audio_start_time = kZeroTime;
  invalid_result.timing_information->audio_end_time = base::Seconds(1);
  invalid_result.timing_information->hypothesis_parts =
      std::vector<media::HypothesisParts>();
  auto& hypothesis_parts =
      invalid_result.timing_information->hypothesis_parts.value();
  // Invalid hypothesis parts (outside the range audio_start_time to
  // audio_end_time):
  hypothesis_parts.emplace_back(std::vector<std::string>({"hello"}),
                                base::Seconds(-1));
  hypothesis_parts.emplace_back(std::vector<std::string>({"world"}),
                                base::Seconds(2));
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionResult::Serialize(&invalid_result);
  media::SpeechRecognitionResult output;
  EXPECT_FALSE(media::mojom::SpeechRecognitionResult::Deserialize(
      std::move(data), &output));
}

TEST(SpeechRecognitionResultStructTraitsTest, WithValidHypothesisParts) {
  media::SpeechRecognitionResult valid_result("hello world", true);
  valid_result.timing_information = media::TimingInformation();
  valid_result.timing_information->audio_start_time = kZeroTime;
  valid_result.timing_information->audio_end_time = base::Seconds(1);
  valid_result.timing_information->hypothesis_parts =
      std::vector<media::HypothesisParts>();
  auto& hypothesis_parts =
      valid_result.timing_information->hypothesis_parts.value();
  hypothesis_parts.emplace_back(std::vector<std::string>({"hello"}),
                                base::Seconds(0));
  hypothesis_parts.emplace_back(std::vector<std::string>({"world"}),
                                base::Seconds(1));
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionResult::Serialize(&valid_result);
  media::SpeechRecognitionResult output;
  EXPECT_TRUE(media::mojom::SpeechRecognitionResult::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(valid_result, output);
}

TEST(SpeechRecognitionResultStructTraitsTest,
     WithInvalidOriginatingMediaTimestamps) {
  constexpr auto verify_serialization_round_trip_fails =
      [](media::MediaTimestampRange invalid_range) {
        media::SpeechRecognitionResult invalid_result("hello world", true);
        invalid_result.timing_information = media::TimingInformation();
        invalid_result.timing_information->audio_start_time = kZeroTime;
        invalid_result.timing_information->audio_end_time = base::Seconds(1);
        invalid_result.timing_information->originating_media_timestamps =
            std::vector<media::MediaTimestampRange>();
        auto& originating_media_timestamps =
            invalid_result.timing_information->originating_media_timestamps
                .value();

        originating_media_timestamps.push_back(std::move(invalid_range));

        std::vector<uint8_t> data =
            media::mojom::SpeechRecognitionResult::Serialize(&invalid_result);
        media::SpeechRecognitionResult output;
        EXPECT_FALSE(media::mojom::SpeechRecognitionResult::Deserialize(
            std::move(data), &output));
      };

  // `start` should always be before `end`
  verify_serialization_round_trip_fails(
      {.start = base::Seconds(10), .end = base::Seconds(5)});

  // A 0 duration range is technically legal, but senders should have removed
  // this range beforehand.
  verify_serialization_round_trip_fails(
      {.start = base::Seconds(10), .end = base::Seconds(10)});
}

TEST(SpeechRecognitionResultStructTraitsTest,
     WithValidOriginatingMediaTimestamps) {
  media::SpeechRecognitionResult invalid_result("hello world", true);
  invalid_result.timing_information = media::TimingInformation();
  invalid_result.timing_information->audio_start_time = kZeroTime;
  invalid_result.timing_information->audio_end_time = base::Seconds(1);
  invalid_result.timing_information->originating_media_timestamps =
      std::vector<media::MediaTimestampRange>();
  auto& originating_media_timestamps =
      invalid_result.timing_information->originating_media_timestamps.value();

  const media::MediaTimestampRange first_range{.start = base::Seconds(5),
                                               .end = base::Seconds(10)};
  // Overlapping ranges are allowed
  const media::MediaTimestampRange second_range{.start = base::Seconds(6),
                                                .end = base::Seconds(11)};
  originating_media_timestamps.push_back(first_range);
  originating_media_timestamps.push_back(second_range);

  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionResult::Serialize(&invalid_result);
  media::SpeechRecognitionResult output;
  EXPECT_TRUE(media::mojom::SpeechRecognitionResult::Deserialize(
      std::move(data), &output));
  const auto& media_timestamps =
      output.timing_information->originating_media_timestamps.value();
  EXPECT_EQ(media_timestamps.size(), 2u);
  EXPECT_EQ(media_timestamps[0], first_range);
  EXPECT_EQ(media_timestamps[1], second_range);
}

}  // namespace media
