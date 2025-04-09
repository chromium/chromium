// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/public/provider/chrome/browser/add_to_calendar/add_to_calendar_api.h"
#import "ios/web/public/web_state.h"

@implementation EnhancedCalendarMediator {
  // The web state that this mediator is associated with.
  raw_ptr<web::WebState> _webState;

  // The config object holding everything needed to complete an Enhanced
  // Calendar model request, and which will hold the parsed response values to
  // present the final "add to calendar" UI.
  EnhancedCalendarConfiguration* _enhancedCalendarConfig;
}

- (instancetype)initWithWebState:(web::WebState*)webState
          enhancedCalendarConfig:
              (EnhancedCalendarConfiguration*)enhancedCalendarConfig {
  self = [super init];
  if (self) {
    _webState = webState;
    _enhancedCalendarConfig = enhancedCalendarConfig;
  }
  return self;
}

- (void)disconnect {
  _webState = nullptr;
  // TODO(crbug.com/405195613): Cancel any in-flight requests.
}

#pragma mark - EnhancedCalendarMutator

- (void)cancelEnhancedCalendarRequestAndDismissBottomSheet {
  // TODO(crbug.com/405195613): Cancel any in-flight requests, and dismiss the
  // bottom sheet.
}

@end
