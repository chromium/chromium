// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PUBLIC_TOOLBAR_OMNIBOX_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PUBLIC_TOOLBAR_OMNIBOX_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_type.h"

/// ToolbarOmniboxConsumer informs omnibox which toolbar is presenting it.
@protocol ToolbarOmniboxConsumer <NSObject>

/// Informs that the unfocused (steady state) omnibox moved to `toolbarType`.
- (void)steadyStateOmniboxMovedToToolbar:(ToolbarType)toolbarType;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PUBLIC_TOOLBAR_OMNIBOX_CONSUMER_H_
