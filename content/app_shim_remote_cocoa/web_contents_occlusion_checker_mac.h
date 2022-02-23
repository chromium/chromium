// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_OCCLUSION_CHECKER_MAC_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_OCCLUSION_CHECKER_MAC_H_

#import <AppKit/AppKit.h>

#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"

@interface WebContentsOcclusionCheckerMac : NSObject

+ (instancetype)sharedInstance;
// Updates the visibility of the webContentsViewCocoa in `window`.
- (void)updateWebContentsVisibilityInWindow:(NSWindow*)window;
// Computes and updates the visibility of the `webContentsViewCocoa`.
- (void)updateWebContentsVisibility:(WebContentsViewCocoa*)webContentsViewCocoa;

@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_OCCLUSION_CHECKER_MAC_H_
