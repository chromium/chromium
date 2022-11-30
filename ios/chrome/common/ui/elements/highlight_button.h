// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_HIGHLIGHT_BUTTON_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_HIGHLIGHT_BUTTON_H_

#import <UIKit/UIKit.h>

// A button that fades opacity on highlight.
@interface HighlightButton : UIButton

// If the button has subviews that are UIVisualEffectViews, fading the opacity
// breaks the visual effect. If this array is non-nil, only the views in this
// array will have their opacities faded. Otherwise, the entire view will fade
// opacity.
@property(nonatomic, strong) NSArray<UIView*>* highlightableViews;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_HIGHLIGHT_BUTTON_H_
