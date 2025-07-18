// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "url/gurl.h"

class Browser;
namespace web {
class WebState;
}

@interface SearchEngineLogoMediator : NSObject <LogoVendor>

// Whether the logo should be multicolor or monochrome.
@property(nonatomic, assign) BOOL usesMonochromeLogo;

// Designated initializer.
- (instancetype)initWithBrowser:(Browser*)browser
                       webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@interface SearchEngineLogoMediator (VisibleForTesting)

// Simulates tapping on the doodle.
- (void)simulateDoodleTapped;
// Sets the destination URL for the doodle tap handler.
- (void)setClickURLText:(const GURL&)url;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_MEDIATOR_SEARCH_ENGINE_LOGO_MEDIATOR_H_
