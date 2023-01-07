// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/fake_voice_search_availability.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/test/providers/voice_search/test_voice_search.h"
#import "testing/gtest/include/gtest/gtest.h"

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
  ios::provider::test::SetVoiceSearchEnabled(enabled);
}

bool FakeVoiceSearchAvailability::IsVoiceOverEnabled() const {
  return voice_over_enabled_;
}
