// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/model/upgrade_utils.h"

#import <Foundation/Foundation.h>
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"

bool IsAppUpToDate() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:kIOSChromeUpToDateKey];
}
