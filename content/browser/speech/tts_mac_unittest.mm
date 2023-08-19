// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/speech/tts_mac.h"

#import <AppKit/AppKit.h>

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(TtsMacTest, CachedVoiceData) {
  std::vector<VoiceData> voices;
  TtsPlatformImplMac::GetInstance()->GetVoices(&voices);

  EXPECT_EQ(voices.size(), AVSpeechSynthesisVoice.speechVoices.count);

  AVSpeechSynthesisVoice* defaultVoice =
      [AVSpeechSynthesisVoice voiceWithLanguage:nil];
  if (defaultVoice) {
    EXPECT_EQ(voices[0].name, base::SysNSStringToUTF8(defaultVoice.name));
  }

  // Simulate the app becoming active, as if the user switched away and back.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSApplicationWillBecomeActiveNotification
                    object:nil];

  // Switching away should have emptied the cache.
  EXPECT_TRUE(TtsPlatformImplMac::VoicesRefForTesting().empty());

  // Reload.
  voices.clear();
  TtsPlatformImplMac::GetInstance()->GetVoices(&voices);

  EXPECT_EQ(voices.size(), AVSpeechSynthesisVoice.speechVoices.count);
}

}  // namespace content
