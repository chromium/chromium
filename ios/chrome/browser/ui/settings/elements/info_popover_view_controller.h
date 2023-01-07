// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_INFO_POPOVER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_INFO_POPOVER_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

// Static popover presenting some information.
@interface InfoPopoverViewController : PopoverLabelViewController

// Init with only a main message shown as the primary label. By default,
// the value of `isPresentingFromButton` is set to YES.
- (instancetype)initWithMessage:(NSString*)message;

// Init with the primary text, the secondary text with an attributed string,
// an `icon` at the left of the secondary text, and a boolean
// `isPresentingFromButton` to determine if it is shown from a button. The size
// of the icon will be of equal height and width. There won't be an icon if
// `icon` is left empty. The icon won't be shown if `secondaryAttributedString`
// is empty.
- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
                                           icon:(UIImage*)icon
                         isPresentingFromButton:(BOOL)isPresentingFromButton
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
    NS_UNAVAILABLE;
- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
                                           icon:(UIImage*)icon NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ELEMENTS_INFO_POPOVER_VIEW_CONTROLLER_H_
