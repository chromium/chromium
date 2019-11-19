// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_CONFIRM_INFOBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_CONFIRM_INFOBAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/fancy_ui/bidi_container_view.h"

// An infobar with an optional message, icon, two action buttons, and a switch.
@interface ConfirmInfoBarView : BidiContainerView

// Label text with links initialized with |stringAsLink:|.
@property(nonatomic, strong, readonly) NSString* markedLabel;

// TODO(crbug.com/302582): rename methods from add* to set*.
// Adds a dismiss button subview.
- (void)addCloseButtonWithTag:(NSInteger)tag
                       target:(id)target
                       action:(SEL)action;

// Adds icon subview. This image will be rendered as a template image.
- (void)addLeftIcon:(UIImage*)image;

// Creates a new string from |string| that is interpreted as a link by
// |addLabel:|. |tag| must not be 0.
+ (NSString*)stringAsLink:(NSString*)string tag:(NSUInteger)tag;

// Adds a message to the infobar that optionaly contains links initialized with
// |stringAsLink:|.
- (void)addLabel:(NSString*)label;

// Adds a message to the infobar that optionaly contains links initialized with
// |stringAsLink:|. |action| is called when a link is clicked, with the tag
// associated with the link passed as a parameter..
- (void)addLabel:(NSString*)label action:(void (^)(NSUInteger))action;

// Adds two buttons to the infobar. Button1 is the primary action of the infobar
// and in Material Design mode is shown with bold colors to reflect this role.
- (void)addButton1:(NSString*)title1
              tag1:(NSInteger)tag1
           button2:(NSString*)title2
              tag2:(NSInteger)tag2
            target:(id)target
            action:(SEL)action;

// Adds a button to the infobar.
- (void)addButton:(NSString*)title
              tag:(NSInteger)tag
           target:(id)target
           action:(SEL)action;

// Adds to the infobar a switch and an adjacent label.
- (void)addSwitchWithLabel:(NSString*)label
                      isOn:(BOOL)isOn
                       tag:(NSInteger)tag
                    target:(id)target
                    action:(SEL)action;

// Adds to the infobar a footer label below the title.
- (void)addFooterLabel:(NSString*)label;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_CONFIRM_INFOBAR_VIEW_H_
