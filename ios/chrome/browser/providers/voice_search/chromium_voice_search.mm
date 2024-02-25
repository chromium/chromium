// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"

#import "ios/chrome/browser/voice/model/voice_search_language.h"

namespace ios {
namespace provider {

bool IsVoiceSearchEnabled() {
  // Voice Search is disabled in Chromium.
  return false;
}

NSArray<VoiceSearchLanguage*>* GetAvailableLanguages() {
  // Add two arbitraty languages to the list so that options show up in the
  // voice search settings page.
  return @[
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"en-US"
                                        displayName:@"English (US)"
                             localizationPreference:nil],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"fr"
                                        displayName:@"French"
                             localizationPreference:nil],
  ];
}

id<VoiceSearchController> CreateVoiceSearchController(Browser* browser) {
  // Should not be called as IsVoiceSearchEnabled() returns false.
  return nil;
}

}  // namespace provider
}  // namespace ios
