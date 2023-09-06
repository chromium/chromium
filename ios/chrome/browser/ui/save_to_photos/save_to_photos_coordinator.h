// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class GURL;
@protocol SigninPresenter;
@protocol SystemIdentity;

namespace web {
struct Referrer;
class WebState;
}  // namespace web

@interface SaveToPhotosCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  imageURL:(const GURL&)imageURL
                                  referrer:(const web::Referrer&)referrer
                                  webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_COORDINATOR_H_
