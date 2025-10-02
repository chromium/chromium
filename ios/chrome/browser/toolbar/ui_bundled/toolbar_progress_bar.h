// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TOOLBAR_PROGRESS_BAR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TOOLBAR_PROGRESS_BAR_H_

#import <UIKit/UIKit.h>

// Progress bar for the toolbar, that indicate that the progress is about the
// page load when read via voice over.
@interface ToolbarProgressBar : UIProgressView

// Sets the hidden state, with an optional animation and completion block.
- (void)setHidden:(BOOL)hidden
         animated:(BOOL)animated
       completion:(void (^)(BOOL finished))completion;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TOOLBAR_PROGRESS_BAR_H_
