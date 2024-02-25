// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/voice_search/test_voice_search.h"

#import "ios/chrome/browser/voice/model/voice_search_language.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"

namespace ios {
namespace provider {
namespace test {
namespace {

// Whether Voice Search is enabled.
bool g_voice_search_enabled = false;

}  // anonymous namespace

void SetVoiceSearchEnabled(bool enabled) {
  g_voice_search_enabled = enabled;
}

}  // namespace test

bool IsVoiceSearchEnabled() {
  // Tests can control whether Voice Search is enabled.
  return test::g_voice_search_enabled;
}

NSArray<VoiceSearchLanguage*>* GetAvailableLanguages() {
  return @[
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"en-US"
                                        displayName:@"English (US)"
                             localizationPreference:nil],
  ];
}

id<VoiceSearchController> CreateVoiceSearchController(Browser* browser) {
  return nil;
}

}  // namespace provider
}  // namespace ios
