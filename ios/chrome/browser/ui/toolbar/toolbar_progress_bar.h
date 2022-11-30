// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PROGRESS_BAR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PROGRESS_BAR_H_

#import <MaterialComponents/MaterialProgressView.h>

// Progress bar for the toolbar, that indicate that the progress is about the
// page load when read via voice over.
@interface ToolbarProgressBar : MDCProgressView
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_PROGRESS_BAR_H_
