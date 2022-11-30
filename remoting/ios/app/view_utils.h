// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_VIEW_UTILS_H_
#define REMOTING_IOS_APP_VIEW_UTILS_H_

#import <UIKit/UIKit.h>

namespace remoting {

// Returns the current topmost presenting view controller of the app.
UIViewController* TopPresentingVC();

// Returns the proper safe area layout guide for iOS 11+; returns a dumb layout
// guide for older OS versions that exactly matches the anchors of the view.
UILayoutGuide* SafeAreaLayoutGuideForView(UIView* view);

// Returns the proper safe area insets for iOS 11+; returns empty insets for
// older OS versions.
UIEdgeInsets SafeAreaInsetsForView(UIView* view);

// Posts a delayed accessibility announcement so that it doesn't interrupt with
// the current announcing speech.
void PostDelayedAccessibilityNotification(NSString* announcement);

// Sets the a11y label of the UIBarButtonItem according to the image it holds.
void SetAccessibilityInfoFromImage(UIBarButtonItem* button);

// Sets the a11y label of the UIButton according to the image it holds.
void SetAccessibilityInfoFromImage(UIButton* button);

void SetAccessibilityFocusElement(id element);

}  // namespace remoting

#endif  // REMOTING_IOS_APP_VIEW_UTILS_H_
