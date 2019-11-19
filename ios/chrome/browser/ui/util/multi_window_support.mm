// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/multi_window_support.h"

#include "base/ios/ios_util.h"
#include "ios/chrome/browser/ui/util/multi_window_buildflags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsMultiwindowSupported() {
  return BUILDFLAG(IOS_MULTIWINDOW_ENABLED) &&
         base::ios::IsRunningOnIOS13OrLater();
}
