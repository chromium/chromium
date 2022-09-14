// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_TOOLTIP_VIEW_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_TOOLTIP_VIEW_H_

#import <UIKit/UIKit.h>

@class TooltipView;

@protocol TooltipViewDelegate

// Informs the delegate that the tooltip is going to be dismissed.
- (void)tooltipViewWillDismiss:(TooltipView*)tooltipView;

@end

@interface TooltipView : UIView

// Delegate for a tooltip view instance.
@property(nonatomic, weak) id<TooltipViewDelegate> delegate;

// Init with the target and `action` parameter-less selector.
- (instancetype)initWithKeyWindow:(UIView*)keyWindow
                           target:(NSObject*)target
                           action:(SEL)action;

// Shows the tooltip with given `message` below the `view`.
- (void)showMessage:(NSString*)message atBottomOf:(UIView*)view;

// Hides this tooltip.
- (void)hide;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_TOOLTIP_VIEW_H_
