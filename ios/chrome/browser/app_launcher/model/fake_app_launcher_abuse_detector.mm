// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/fake_app_launcher_abuse_detector.h"

@implementation FakeAppLauncherAbuseDetector

- (instancetype)init {
  if ((self = [super init])) {
    _policy = ExternalAppLaunchPolicyAllow;
  }
  return self;
}

- (ExternalAppLaunchPolicy)launchPolicyForURL:(const GURL&)URL
                            fromSourcePageURL:(const GURL&)sourcePageURL {
  return self.policy;
}
@end
