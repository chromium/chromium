// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/speech_recognition_recognition_context_mojom_traits.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(SpeechRecognitionPhraseStructTraitsTest, ValidPhrase) {
  media::SpeechRecognitionPhrase phrase("test phrase", 1.0);
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionPhrase::Serialize(&phrase);
  media::SpeechRecognitionPhrase output;
  EXPECT_TRUE(media::mojom::SpeechRecognitionPhrase::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(phrase, output);
}

TEST(SpeechRecognitionPhraseStructTraitsTest, InvalidEmptyPhrase) {
  media::SpeechRecognitionPhrase phrase("", 1.0);
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionPhrase::Serialize(&phrase);
  media::SpeechRecognitionPhrase output;
  EXPECT_FALSE(media::mojom::SpeechRecognitionPhrase::Deserialize(
      std::move(data), &output));
}

TEST(SpeechRecognitionPhraseStructTraitsTest, InvalidPhraseBoostTooSmall) {
  media::SpeechRecognitionPhrase phrase("test phrase", -0.1);
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionPhrase::Serialize(&phrase);
  media::SpeechRecognitionPhrase output;
  EXPECT_FALSE(media::mojom::SpeechRecognitionPhrase::Deserialize(
      std::move(data), &output));
}

TEST(SpeechRecognitionPhraseStructTraitsTest, InvalidPhraseBoostTooBig) {
  media::SpeechRecognitionPhrase phrase("test phrase", 10.1);
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionPhrase::Serialize(&phrase);
  media::SpeechRecognitionPhrase output;
  EXPECT_FALSE(media::mojom::SpeechRecognitionPhrase::Deserialize(
      std::move(data), &output));
}

TEST(SpeechRecognitionRecognitionContextStructTraitsTest,
     ValidRecognitionContext) {
  media::SpeechRecognitionRecognitionContext context;
  context.phrases.push_back(media::SpeechRecognitionPhrase("test phrase", 0.0));
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionRecognitionContext::Serialize(&context);
  media::SpeechRecognitionRecognitionContext output;
  EXPECT_TRUE(media::mojom::SpeechRecognitionRecognitionContext::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(context, output);
}

TEST(SpeechRecognitionRecognitionContextStructTraitsTest, ValidEmptyPhrases) {
  media::SpeechRecognitionRecognitionContext context;
  std::vector<uint8_t> data =
      media::mojom::SpeechRecognitionRecognitionContext::Serialize(&context);
  media::SpeechRecognitionRecognitionContext output;
  EXPECT_TRUE(media::mojom::SpeechRecognitionRecognitionContext::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(context, output);
}

}  // namespace media
