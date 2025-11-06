// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_DISMISS_ANIMATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_DISMISS_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/ui/composebox_animation_context_provider.h"

// Animator for the custom dismissal of the composebox.
@interface ComposeboxDismissAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>
- (instancetype)initWithContextProvider:
    (id<ComposeboxAnimationContextProvider>)contextProvider;
@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_DISMISS_ANIMATOR_H_
