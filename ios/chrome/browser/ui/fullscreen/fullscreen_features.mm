// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"

#import <Foundation/Foundation.h>
#import "base/command_line.h"
#import "base/feature_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace fullscreen {
namespace features {

BASE_FEATURE(kSmoothScrollingDefault,
             "FullscreenSmoothScrollingDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace fullscreen
