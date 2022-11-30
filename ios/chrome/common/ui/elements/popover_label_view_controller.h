// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_POPOVER_LABEL_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_POPOVER_LABEL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Delegate which handles interactions inside popover.
@protocol PopoverLabelViewControllerDelegate

// Notifies the delegate that a link was tapped.
- (void)didTapLinkURL:(NSURL*)URL;

@end

// Static popover presenting a simple message.
@interface PopoverLabelViewController : UIViewController

// Init with only a main message shown as the primary label.
- (instancetype)initWithMessage:(NSString*)message;

// Init with primary string and an attributed string set to secondary text.
- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString;

// Init with the primary text, the secondary text with an attributed string,
// and a `icon` at the left of the secondary text. The size of the icon will be
// of equal height and width. There won't be an icon if `icon` is left empty.
// The icon won't be shown if `secondaryAttributedString` is empty.
- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
                                           icon:(UIImage*)icon
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@property(nonatomic, weak) id<PopoverLabelViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_POPOVER_LABEL_VIEW_CONTROLLER_H_
