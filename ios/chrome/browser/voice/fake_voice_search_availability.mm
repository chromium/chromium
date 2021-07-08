// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/fake_voice_search_availability.h"

#include "base/memory/ptr_util.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/voice/test_voice_search_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeVoiceSearchAvailability::FakeVoiceSearchAvailability() = default;

FakeVoiceSearchAvailability::~FakeVoiceSearchAvailability() = default;

void FakeVoiceSearchAvailability::SetVoiceOverEnabled(bool enabled) {
  if (voice_over_enabled_ == enabled)
    return;
  voice_over_enabled_ = enabled;
  NSNotification* notification = [NSNotification
      notificationWithName:UIAccessibilityVoiceOverStatusDidChangeNotification
                    object:nil];
  [NSNotificationCenter.defaultCenter postNotification:notification];
}

void FakeVoiceSearchAvailability::SetVoiceProviderEnabled(bool enabled) {
  TestVoiceSearchProvider* voice_provider =
      static_cast<TestVoiceSearchProvider*>(
          ios::TestChromeBrowserProvider::GetTestProvider()
              .GetVoiceSearchProvider());
  voice_provider->set_voice_search_enabled(enabled);
}

bool FakeVoiceSearchAvailability::IsVoiceOverEnabled() const {
  return voice_over_enabled_;
}
