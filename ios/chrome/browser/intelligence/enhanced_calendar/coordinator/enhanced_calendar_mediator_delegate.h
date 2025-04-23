// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_MEDIATOR_DELEGATE_H_

@class EnhancedCalendarConfiguration;
@class EnhancedCalendarMediator;

// Delegate for the EnhancedCalendarMediator.
@protocol EnhancedCalendarMediatorDelegate

// Presents the "add to calendar" UI with the given configuration.
- (void)presentAddToCalendar:(EnhancedCalendarMediator*)mediator
                      config:(EnhancedCalendarConfiguration*)config;

// Cancels any in-flight requests and dismisses the view controller (bottom
// sheet).
- (void)cancelRequestsAndDismissViewController:
    (EnhancedCalendarMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_COORDINATOR_ENHANCED_CALENDAR_MEDIATOR_DELEGATE_H_
