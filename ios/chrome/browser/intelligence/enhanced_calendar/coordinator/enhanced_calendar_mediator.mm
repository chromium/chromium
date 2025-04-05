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

  // The integration provider for the "add to calendar" experience.
  ios::provider::AddToCalendarIntegrationProvider _integrationProvider;
}

- (instancetype)initWithWebState:(web::WebState*)webState
             integrationProvider:
                 (ios::provider::AddToCalendarIntegrationProvider)
                     integrationProvider {
  self = [super init];
  if (self) {
    _webState = webState;
    _integrationProvider = integrationProvider;
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
