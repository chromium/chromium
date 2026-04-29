// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_mediator.h"

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_consumer.h"

@implementation TravelInfoMediator

- (void)disconnect {
  self.consumer = nil;
}

@end
