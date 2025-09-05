// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_BACKGROUND_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

@interface MagicStackModuleBackgroundView : UIView

// Fades the view in.
- (void)fadeIn;

// Fades the view out.
- (void)fadeOut;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_BACKGROUND_VIEW_H_
