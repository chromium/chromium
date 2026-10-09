// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_ZERO_STATE_BACKGROUND_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_ZERO_STATE_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

// A view that renders the zero-state background halo gradient effect.
@interface GeminiContainerZeroStateBackgroundView : UIView

// Initializes the zero-state background view.
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_ZERO_STATE_BACKGROUND_VIEW_H_
