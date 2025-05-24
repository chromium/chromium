// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_UI_ENHANCED_CALENDAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_UI_ENHANCED_CALENDAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@protocol EnhancedCalendarMutator;

// The view controller for the Enhanced Calendar feature's UI, presented as a
// bottom sheet.
@interface EnhancedCalendarViewController
    : BottomSheetViewController <ConfirmationAlertActionHandler>

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<EnhancedCalendarMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_UI_ENHANCED_CALENDAR_VIEW_CONTROLLER_H_
