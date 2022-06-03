// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_ACTION_VIEW_H_
#define IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_ACTION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/elements/highlight_button.h"

// View for an action to launch the app from the widget. Represented as a
// circular icon and a label. When tapped it calls |actionSelector| in |target|.
@interface SearchActionView : HighlightButton

// Designated initializer, creates the action view with a |target| and
// |selector| to act on. The image with name |imageName| is shown in the
// circular icon. The |title| is shown beneath the icon.
- (instancetype)initWithActionTarget:(id)target
                      actionSelector:(SEL)actionSelector
                               title:(NSString*)title
                           imageName:(NSString*)imageName
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_ACTION_VIEW_H_
