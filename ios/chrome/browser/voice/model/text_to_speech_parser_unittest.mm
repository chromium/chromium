// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/voice/model/text_to_speech_parser.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Expose internal parser function for testing.
NSData* ExtractVoiceSearchAudioDataFromPageHTML(NSString* pageHTML);

namespace {
const char kExpectedDecodedData[] = "testaudo32oio";
NSString* const kValidVoiceSearchHTML =
    @"<script>(function(){var _a_tts='dGVzdGF1ZG8zMm9pbw==';var _m_tts= {}}";
NSString* const kInvalidVoiceSearchHTML = @"no TTS data";
}  // namespace

using TextToSpeechParser = PlatformTest;

TEST_F(TextToSpeechParser, ExtractAudioDataValid) {
  NSData* result =
      ExtractVoiceSearchAudioDataFromPageHTML(kValidVoiceSearchHTML);

  EXPECT_NSNE(result, nil);

  NSData* expectedData =
      [NSData dataWithBytes:&kExpectedDecodedData[0]
                     length:sizeof(kExpectedDecodedData) - 1];
  EXPECT_NSEQ(expectedData, result);
}

TEST_F(TextToSpeechParser, ExtractAudioDataNotFound) {
  NSData* result =
      ExtractVoiceSearchAudioDataFromPageHTML(kInvalidVoiceSearchHTML);
  EXPECT_NSEQ(result, nil);
}
