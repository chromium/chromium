// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_FAKE_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_FAKE_BROWSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/main/ui/browser_layout_consumer.h"

// Fake ViewController implementing BrowserLayoutConsumer for testing.
@interface FakeBrowserViewController : UIViewController <BrowserLayoutConsumer>

// Direct access to the property from the protocol.
@property(nonatomic, assign) CGFloat topToolbarInset;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_FAKE_BROWSER_VIEW_CONTROLLER_H_
