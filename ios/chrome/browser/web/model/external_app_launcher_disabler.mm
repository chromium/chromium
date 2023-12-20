// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/external_app_launcher_disabler.h"

@implementation ExternalAppLauncherDisabler

- (BOOL)openURL:(const GURL&)gurl {
  return NO;
}

@end
