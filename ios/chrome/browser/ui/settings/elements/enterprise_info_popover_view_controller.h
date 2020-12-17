// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_ENTERPRISE_INFO_POPOVER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_ENTERPRISE_INFO_POPOVER_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

// Static popover presenting the information of the enterprise.
@interface EnterpriseInfoPopoverViewController : PopoverLabelViewController

// Initializes the popover with default primary text, and secondary text based
// on the given |enterpriseName|.
- (instancetype)initWithEnterpriseName:(NSString*)enterpriseName;

// Initializes the popover with the given |message| as primary text, and
// secondary text based on the given |enterpriseName|.
- (instancetype)initWithMessage:(NSString*)message
                 enterpriseName:(NSString*)enterpriseName
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithMessage:(NSString*)message NS_UNAVAILABLE;
- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_ENTERPRISE_INFO_POPOVER_VIEW_CONTROLLER_H_
