// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_PUBLIC_TOOLBAR_OMNIBOX_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_PUBLIC_TOOLBAR_OMNIBOX_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_type.h"

/// ToolbarOmniboxConsumer informs omnibox which toolbar is presenting it.
@protocol ToolbarOmniboxConsumer <NSObject>

/// Informs that the unfocused (steady state) omnibox moved to `toolbarType`.
- (void)steadyStateOmniboxMovedToToolbar:(ToolbarType)toolbarType;

/// Sets whether the underlying page is NTP.
- (void)setIsNTP:(BOOL)isNTP;

/// Sets the preferred omnibox position.
- (void)setPreferredOmniboxPosition:(ToolbarType)preferredOmniboxPosition;

/// Sets the offset to be applied in the bottom of the popup when using the
/// bottom omnibox.
- (void)setBottomOmniboxOffsetForPopup:(CGFloat)bottomOffset;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_PUBLIC_TOOLBAR_OMNIBOX_CONSUMER_H_
