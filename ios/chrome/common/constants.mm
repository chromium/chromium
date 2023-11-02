// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const NSTimeInterval kSecurePasteboardExpiration = 60 * 20;  // 20 minutes.
const CGFloat kSmallDeviceThreshold = 22.0;