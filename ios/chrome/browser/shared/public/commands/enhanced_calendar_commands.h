// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ENHANCED_CALENDAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ENHANCED_CALENDAR_COMMANDS_H_

@class EnhancedCalendarConfiguration;

// Commands to show/hide the Enhanced Calendar bottom sheet.
@protocol EnhancedCalendarCommands <NSObject>

// Shows the Enhanced Calendar bottom sheet for the current WebState.
- (void)showEnhancedCalendarWithConfig:
    (EnhancedCalendarConfiguration*)enhancedCalendarConfig;

// Hides the Enhanced Calendar bottom sheet. Will also cancel any in-flight
// Enhanced Calendar model requests
- (void)hideEnhancedCalendarBottomSheet;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ENHANCED_CALENDAR_COMMANDS_H_
