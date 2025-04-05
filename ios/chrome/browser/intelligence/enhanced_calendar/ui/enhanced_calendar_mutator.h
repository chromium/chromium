// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_UI_ENHANCED_CALENDAR_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_UI_ENHANCED_CALENDAR_MUTATOR_H_

// Mutator protocol for the view controller to communicate with the
// EnhancedCalendarMediator.
@protocol EnhancedCalendarMutator

// Cancels any in-flight EnhancedCalendar requests and dismisses the bottom
// sheet. User-triggered.
- (void)cancelEnhancedCalendarRequestAndDismissBottomSheet;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_UI_ENHANCED_CALENDAR_MUTATOR_H_
