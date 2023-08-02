// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_main_starter.h"

#import "ios/chrome/app/startup/ios_chrome_main.h"

@implementation ChromeMainStarter

+ (std::unique_ptr<IOSChromeMain>)startChromeMain {
  return std::make_unique<IOSChromeMain>();
}

@end
