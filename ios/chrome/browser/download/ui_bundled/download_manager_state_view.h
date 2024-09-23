// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_STATE_VIEW_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_STATE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/ui_bundled/download_manager_state.h"

// View that display relevant icon for DownloadManagerState. This view have a
// fixed size which can not be changed.
@interface DownloadManagerStateView : UIImageView

// Updates the view according to the given state.
- (void)setState:(DownloadManagerState)state;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_STATE_VIEW_H_
