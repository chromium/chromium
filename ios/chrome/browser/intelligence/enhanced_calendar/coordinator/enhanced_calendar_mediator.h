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

@class EnhancedCalendarConfiguration;

@protocol EnhancedCalendarMediatorDelegate;

// The mediator for the Enhanced Calendar feature's UI.
@interface EnhancedCalendarMediator : NSObject <EnhancedCalendarMutator>

// The delegate for this mediator.
@property(nonatomic, weak) id<EnhancedCalendarMediatorDelegate> delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
          enhancedCalendarConfig:
              (EnhancedCalendarConfiguration*)enhancedCalendarConfig
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

// Creates the request proto and executes the Enhanced Calendar request.
- (void)startEnhancedCalendarRequest;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_MEDIATOR_H_
