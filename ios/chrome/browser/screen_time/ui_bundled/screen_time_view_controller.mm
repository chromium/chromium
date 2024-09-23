// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen_time/ui_bundled/screen_time_view_controller.h"

#import "ios/web/public/thread/web_thread.h"

@implementation ScreenTimeViewController
// The ScreenTimeConsumer implementation comes from the STWebpageController
// parent class.

+ (instancetype)sharedInstance {
  // make this a main-thread only class to mitigate risks of
  // production issues.
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  static ScreenTimeViewController* sharedInstance =
      [[ScreenTimeViewController alloc] init];
  return sharedInstance;
}

+ (instancetype)sharedOTRInstance {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  static ScreenTimeViewController* sharedOTRInstance =
      [[ScreenTimeViewController alloc] init];
  return sharedOTRInstance;
}
@end
