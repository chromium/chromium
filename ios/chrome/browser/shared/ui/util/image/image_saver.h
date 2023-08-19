// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_SAVER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_SAVER_H_

#import <UIKit/UIKit.h>

#include "components/image_fetcher/core/request_metadata.h"

class Browser;
class GURL;
namespace web {
class WebState;
struct Referrer;
}  // namespace web

// Object saving images to the system's album.
@interface ImageSaver : NSObject

// Init the ImageSaver.
- (instancetype)initWithBrowser:(Browser*)browser;

// Fetches and saves the image at `url` to the system's album. `web_state` is
// used for fetching image data by JavaScript and must not be nullptr.
// `referrer` is used for download. `baseViewController` used to display alerts.
- (void)saveImageAtURL:(const GURL&)URL
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState
    baseViewController:(UIViewController*)baseViewController;

// Stops the image saver.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IMAGE_IMAGE_SAVER_H_
