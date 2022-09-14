// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/voice_search_keyboard_accessory_button.h"

#import "base/check.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VoiceSearchKeyboardAccessoryButton () <
    VoiceSearchAvailabilityObserver> {
  std::unique_ptr<VoiceSearchAvailability> _availability;
}
@end

@implementation VoiceSearchKeyboardAccessoryButton

- (instancetype)initWithVoiceSearchAvailability:
    (std::unique_ptr<VoiceSearchAvailability>)availability {
  if (self = [super initWithFrame:CGRectZero]) {
    _availability = std::move(availability);
    DCHECK(_availability);
    _availability->AddObserver(self);
  }
  return self;
}

- (void)dealloc {
  _availability->RemoveObserver(self);
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [self updateEnabledState];
}

#pragma mark - VoiceSearchAvailabilityObserver

- (void)voiceSearchAvailability:(VoiceSearchAvailability*)availability
            updatedAvailability:(BOOL)available {
  [self updateEnabledState];
}

#pragma mark - Private

// Updates the button's enabled state according to its voice search
// availability.
- (void)updateEnabledState {
  self.enabled = _availability->IsVoiceSearchAvailable();
}

@end
