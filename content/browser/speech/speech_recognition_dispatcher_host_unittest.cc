// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_dispatcher_host.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(SpeechRecognitionDispatcherHostTest, GetAcceptedLanguages) {
  // Test with a single language.
  EXPECT_EQ("es",
            SpeechRecognitionDispatcherHost::GetAcceptedLanguages("", "es"));

  // Test with multiple languages.
  EXPECT_EQ("es", SpeechRecognitionDispatcherHost::GetAcceptedLanguages(
                      "", "es,en-GB;q=0.8"));

  // Test with language specified.
  EXPECT_EQ("fr", SpeechRecognitionDispatcherHost::GetAcceptedLanguages(
                      "fr", "es,en-GB;q=0.8"));

  // Test with empty language and accept_language.
  EXPECT_EQ("en-US",
            SpeechRecognitionDispatcherHost::GetAcceptedLanguages("", ""));
}

}  // namespace content
