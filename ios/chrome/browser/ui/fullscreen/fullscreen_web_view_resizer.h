// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_RESIZER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_RESIZER_H_

#import <UIKit/UIKit.h>

class FullscreenModel;

namespace web {
class WebState;
}

// Resizer for the fullscreen view. This object is taking care of resizing the
// WebView associated with a WebState to have it compatible with the fullscreen
// feature.
@interface FullscreenWebViewResizer : NSObject

// Initializes the object with the fullscreen `model`, used to get the
// information about the state of fullscreen.
- (instancetype)initWithModel:(FullscreenModel*)model NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// WebState currently displayed.
@property(nonatomic, assign) web::WebState* webState;

// Whether the content offset should be matching the frame changes. Default is
// YES.
@property(nonatomic, assign) BOOL compensateFrameChangeByOffset;

// Updates the WebState view, based on the current state of the webState and the
// model.
- (void)updateForCurrentState;

// Force the updates of the WebView to `progress`. `progress` should be between
// 0 and 1, 0 meaning that the application is in fullscreen, 1 that it is out of
// fullscreen.
- (void)forceToUpdateToProgress:(CGFloat)progress;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_RESIZER_H_
