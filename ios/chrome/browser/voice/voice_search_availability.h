// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_VOICE_SEARCH_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_VOICE_VOICE_SEARCH_AVAILABILITY_H_

#import <UIKit/UIKit.h>

#import "base/ios/crb_protocol_observers.h"

@protocol VoiceSearchAvailabilityObserver;

// Helper object that determines the availability of the voice search feature.
// Voice search is only enabled if:
// - the `voice_search` API is enabled, and
// - VoiceOver is disabled.
class VoiceSearchAvailability {
 public:
  VoiceSearchAvailability();
  virtual ~VoiceSearchAvailability();

  // Adds and removes |observer|.
  void AddObserver(id<VoiceSearchAvailabilityObserver> observer);
  void RemoveObserver(id<VoiceSearchAvailabilityObserver> observer);

  // Returns whether voice search is available.
  bool IsVoiceSearchAvailable() const;

 protected:
  // Returns whether VoiceOver is enabled.  The default implementation queries
  // UIKit.
  virtual bool IsVoiceOverEnabled() const;

 private:
  // Setter for |voice_over_enabled_|.
  void SetIsVoiceOverEnabled(bool enabled);

  bool voice_over_enabled_ = false;
  id notification_observer_ = nil;
  CRBProtocolObservers<VoiceSearchAvailabilityObserver>* observers_ = nil;
};

// Interface for objects interested in changes to the availability of voice
// search.
@protocol VoiceSearchAvailabilityObserver <NSObject>

// Called when |availability|'s IsVoiceSearchAvailable() value has changed to
// |available|.
- (void)voiceSearchAvailability:(VoiceSearchAvailability*)availability
            updatedAvailability:(BOOL)available;

@end

#endif  // IOS_CHROME_BROWSER_VOICE_VOICE_SEARCH_AVAILABILITY_H_
