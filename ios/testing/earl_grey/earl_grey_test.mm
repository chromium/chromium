// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/earl_grey_test.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_1)

id<GREYMatcher> grey_kindOfClassName(NSString* name) {
  Class klass = NSClassFromString(name);
  DCHECK(klass);
  return grey_kindOfClass(klass);
}

#endif
