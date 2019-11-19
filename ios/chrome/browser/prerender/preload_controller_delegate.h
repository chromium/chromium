// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class PreloadController;

// A protocol implemented by a delegate of PreloadController
@protocol PreloadControllerDelegate

// WebState from which preload controller should copy the session history.
// This web state will be replaced on successful preload.
- (web::WebState*)webStateToReplace;

// Should preload controller request a desktop site.
- (BOOL)preloadShouldUseDesktopUserAgent;

@end

#endif  // IOS_CHROME_BROWSER_PRERENDER_PRELOAD_CONTROLLER_DELEGATE_H_
