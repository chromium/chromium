// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_availability.h"

#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

VoiceSearchAvailability::VoiceSearchAvailability()
    : observers_(static_cast<id>([CRBProtocolObservers
          observersWithProtocol:@protocol(VoiceSearchAvailabilityObserver)])) {
  voice_over_enabled_ = IsVoiceOverEnabled();
  notification_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIAccessibilityVoiceOverStatusDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull note) {
                // This block will be unregistered in VoiceSearchAvailability's
                // destructor, thus the captured "this" won't outlive the
                // current instance which makes this code safe.
                SetIsVoiceOverEnabled(IsVoiceOverEnabled());
              }];
}

VoiceSearchAvailability::~VoiceSearchAvailability() {
  [NSNotificationCenter.defaultCenter removeObserver:notification_observer_];
}

void VoiceSearchAvailability::AddObserver(
    id<VoiceSearchAvailabilityObserver> observer) {
  [observers_ addObserver:observer];
}

void VoiceSearchAvailability::RemoveObserver(
    id<VoiceSearchAvailabilityObserver> observer) {
  [observers_ removeObserver:observer];
}

bool VoiceSearchAvailability::IsVoiceSearchAvailable() const {
  return ios::provider::IsVoiceSearchEnabled();
}

bool VoiceSearchAvailability::IsVoiceOverEnabled() const {
  return UIAccessibilityIsVoiceOverRunning();
}

void VoiceSearchAvailability::SetIsVoiceOverEnabled(bool enabled) {
  if (voice_over_enabled_ == enabled)
    return;

  bool previously_available = IsVoiceSearchAvailable();
  voice_over_enabled_ = enabled;
  bool available = IsVoiceSearchAvailable();
  if (available == previously_available)
    return;

  [observers_ voiceSearchAvailability:this updatedAvailability:available];
}
