// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_voice_search_provider.h"

#import "ios/chrome/browser/voice/voice_search_language.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumVoiceSearchProvider::ChromiumVoiceSearchProvider() {}

ChromiumVoiceSearchProvider::~ChromiumVoiceSearchProvider() {}

bool ChromiumVoiceSearchProvider::IsVoiceSearchEnabled() const {
  return false;
}

NSArray* ChromiumVoiceSearchProvider::GetAvailableLanguages() const {
  // Add two arbitrary languages to the list, so that options show up in the
  // voice search settings page.
  VoiceSearchLanguage* en_US =
      [[VoiceSearchLanguage alloc] initWithIdentifier:@"en-US"
                                          displayName:@"English (US)"
                               localizationPreference:nil];
  VoiceSearchLanguage* fr =
      [[VoiceSearchLanguage alloc] initWithIdentifier:@"fr"
                                          displayName:@"French"
                               localizationPreference:nil];

  return @[ en_US, fr ];
}
