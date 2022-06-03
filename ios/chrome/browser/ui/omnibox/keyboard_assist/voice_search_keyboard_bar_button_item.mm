// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/voice_search_keyboard_bar_button_item.h"

#import "ios/chrome/browser/voice/voice_search_availability.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VoiceSearchKeyboardBarButtonItem () <
    VoiceSearchAvailabilityObserver> {
  std::unique_ptr<VoiceSearchAvailability> _availability;
}
@end

@implementation VoiceSearchKeyboardBarButtonItem

- (instancetype)initWithImage:(UIImage*)image
                        style:(UIBarButtonItemStyle)style
                       target:(id)target
                       action:(SEL)action
      voiceSearchAvailability:
          (std::unique_ptr<VoiceSearchAvailability>)availability {
  if (self = [super init]) {
    self.image = image;
    self.style = style;
    self.target = target;
    self.action = action;
    _availability = std::move(availability);
    _availability->AddObserver(self);
    [self updateEnabledState];
  }
  return self;
}

- (void)dealloc {
  _availability->RemoveObserver(self);
}

#pragma mark - VoiceSearchAvailabilityObserver

- (void)voiceSearchAvailability:(VoiceSearchAvailability*)availability
            updatedAvailability:(BOOL)available {
  [self updateEnabledState];
}

#pragma mark - Private

// Updates the item's enabled state according to its voice search availability.
- (void)updateEnabledState {
  self.enabled = _availability->IsVoiceSearchAvailable();
}

@end
