// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/speech/tts_mac.h"

#import <AppKit/AppKit.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(TtsMacTest, CachedVoiceData) {
  base::test::TaskEnvironment task_environment;
  TtsPlatformImplMac* tts = TtsPlatformImplMac::GetInstance();

  // The voice list is loaded in the background; until then the platform is
  // not initialized and reports no voices.
  ASSERT_TRUE(
      base::test::RunUntil([&] { return tts->PlatformImplInitialized(); }));

  std::vector<VoiceData> voices;
  tts->GetVoices(&voices);
  EXPECT_EQ(voices.size(), AVSpeechSynthesisVoice.speechVoices.count);

  AVSpeechSynthesisVoice* default_voice =
      TtsPlatformImplMacBackgroundWorker().GetSystemDefaultVoice();
  if (default_voice && !voices.empty()) {
    EXPECT_EQ(voices[0].native_voice_identifier,
              base::SysNSStringToUTF8(default_voice.identifier));
    EXPECT_TRUE(voices[0].name.starts_with(
        base::SysNSStringToUTF8(default_voice.name)));
  }

  // A refresh reloads in the background and keeps serving the cached list in
  // the meantime.
  tts->RefreshVoices();
  std::vector<VoiceData> voices_during_reload;
  tts->GetVoices(&voices_during_reload);
  EXPECT_EQ(voices_during_reload.size(), voices.size());
}

}  // namespace content
