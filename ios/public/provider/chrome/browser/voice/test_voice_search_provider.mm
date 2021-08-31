// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/voice/test_voice_search_provider.h"

#import "ios/public/provider/chrome/browser/voice/voice_search_language.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestVoiceSearchProvider::TestVoiceSearchProvider() {}

TestVoiceSearchProvider::~TestVoiceSearchProvider() {}

bool TestVoiceSearchProvider::IsVoiceSearchEnabled() const {
  return voice_search_enabled_;
}

NSArray* TestVoiceSearchProvider::GetAvailableLanguages() const {
  VoiceSearchLanguage* en =
      [[VoiceSearchLanguage alloc] initWithIdentifier:@"en-US"
                                          displayName:@"English (US)"
                               localizationPreference:nil];
  return @[ en ];
}
