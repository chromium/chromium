// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_ENTERPRISE_INFO_POPOVER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_ENTERPRISE_INFO_POPOVER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/elements/info_popover_view_controller.h"

// Static popover presenting the information of the enterprise.
@interface EnterpriseInfoPopoverViewController : InfoPopoverViewController

// Initializes the popover with default primary text, and secondary text based
// on the given `enterpriseName`.
- (instancetype)initWithEnterpriseName:(NSString*)enterpriseName;

// Initializes the popover with default behavior with the given `message` as
// primary text, and secondary text based on the given `enterpriseName`.
// By default, the value of `isPresentingFromButton` is set to YES and the value
// of `addLearnMoreLink` is set to YES.
- (instancetype)initWithMessage:(NSString*)message
                 enterpriseName:(NSString*)enterpriseName;

// Initializes the popover with the given `message` as primary text, a secondary
// text based on the given `enterpriseName`, a boolean `isPresentingFromButton`
// to determine if it is shown from a button and a boolean `addLearnMoreLink`
// to add a "Learn More" link.
- (instancetype)initWithMessage:(NSString*)message
                 enterpriseName:(NSString*)enterpriseName
         isPresentingFromButton:(BOOL)isPresentingFromButton
               addLearnMoreLink:(BOOL)addLearnMoreLink
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithMessage:(NSString*)message NS_UNAVAILABLE;
- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
                                           icon:(UIImage*)icon
                         isPresentingFromButton:(BOOL)isPresentingFromButton
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_ENTERPRISE_INFO_POPOVER_VIEW_CONTROLLER_H_
