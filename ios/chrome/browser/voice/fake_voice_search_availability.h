// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_FAKE_VOICE_SEARCH_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_VOICE_FAKE_VOICE_SEARCH_AVAILABILITY_H_

#import "ios/chrome/browser/voice/voice_search_availability.h"

// Fake version of VoiceSearchAvailability for use in tests.
class FakeVoiceSearchAvailability : public VoiceSearchAvailability {
 public:
  FakeVoiceSearchAvailability();
  ~FakeVoiceSearchAvailability() override;

  // Setter for whether VoiceOver is enabled.  Disabled by default.  Posts a
  // VoiceOver change notification when the value is updated.
  void SetVoiceOverEnabled(bool enabled);
  // Setter for whether voice search is enabled for the `voice_search` API.
  // Disabled by default.  Must be used in a test fixture that uses a
  // TestChromeBrowserProvider.
  void SetVoiceProviderEnabled(bool enabled);

 protected:
  // VoiceSearchAvailability:
  bool IsVoiceOverEnabled() const override;

 private:
  // Whether voice over is enabled.  Used as return value for
  // IsVoiceOverEnabled().
  bool voice_over_enabled_ = false;
};

#endif  // IOS_CHROME_BROWSER_VOICE_FAKE_VOICE_SEARCH_AVAILABILITY_H_
