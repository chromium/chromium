// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/enhanced_calendar/ui/enhanced_calendar_mutator.h"

namespace web {
class WebState;
}  // namespace web

namespace ios::provider {
enum class AddToCalendarIntegrationProvider;
}  // namespace ios::provider

// The mediator for the Enhanced Calendar feature's UI.
@interface EnhancedCalendarMediator : NSObject <EnhancedCalendarMutator>

- (instancetype)initWithWebState:(web::WebState*)webState
             integrationProvider:
                 (ios::provider::AddToCalendarIntegrationProvider)
                     integrationProvider NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_MEDIATOR_H_
