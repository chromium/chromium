// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_CHROME_MAIN_STARTER_H_
#define IOS_CHROME_APP_STARTUP_CHROME_MAIN_STARTER_H_

#include <memory>

#import <UIKit/UIKit.h>

class IOSChromeMain;

@interface ChromeMainStarter : NSObject

// Setup and initialization that is needed by common Chrome code. This should be
// called only once during app startup (or shortly after launch). Returns the
// object that drives startup/shutdown logic.
+ (std::unique_ptr<IOSChromeMain>)startChromeMain;

@end
#endif  // IOS_CHROME_APP_STARTUP_CHROME_MAIN_STARTER_H_
