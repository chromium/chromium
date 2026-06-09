// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_COPIER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_COPIER_H_

#import <UIKit/UIKit.h>

#import <string>

#import "url/origin.h"

class Browser;
class GURL;

namespace web {
struct Referrer;
class WebState;
}  // namespace web

// Object copying images to the system's pasteboard.
@interface ImageCopier : NSObject

// Init the ImageCopier.
- (instancetype)initWithBrowser:(Browser*)browser;

// Copies the image at `url`. `web_state` is used for fetching image data by
// JavaScript. `referrer` is used for download. `baseViewController` used to
// display alerts.
- (void)copyImageAtURL:(const GURL&)url
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState
               frameID:(const std::string&)frameID
           frameOrigin:(const url::Origin&)frameOrigin
    baseViewController:(UIViewController*)baseViewController;

// Stops the image copier.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_COPIER_H_
