// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/at_exit.h"
#import "ios/showcase/core/app_delegate.h"

int main(int argc, char* argv[]) {
  // This needs to be stack allocated and live for the lifetime of
  // the app.
  base::AtExitManager at_exit;

  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil,
                             NSStringFromClass([AppDelegate class]));
  }
}
