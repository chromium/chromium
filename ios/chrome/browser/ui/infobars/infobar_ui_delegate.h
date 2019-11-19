// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_UI_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/infobars/infobar_type.h"

class InfoBarControllerDelegate;

// Protocol to communicate with the Infobar container.
@protocol InfobarUIDelegate

// Removes the view from the View Hierarchy.
- (void)removeView;

// Removes the view from the View Hierarchy, and deletes the backing Infobar
// object.
- (void)detachView;

// The InfobarControllerDelegate.
@property(nonatomic, assign) InfoBarControllerDelegate* delegate;

// YES if the container should modally present the Infobar.
@property(nonatomic, assign, getter=isPresented) BOOL presented;

// YES if a badge should be displayed for this Infobar.
@property(nonatomic, assign, readonly) BOOL hasBadge;

// The InfobarType for this Infobar.
@property(nonatomic, assign) InfobarType infobarType;

@optional
// The Infobar UIView.
// TODO(crbug.com/927064): Only used in the Legacy implementation.
@property(nonatomic, readonly) UIView* view;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_UI_DELEGATE_H_
