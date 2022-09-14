// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace link_to_text {

const double kLinkGenerationTimeoutInMs = 500.0;
const double kPreconditionsTimeoutInSeconds = 0.1;
const double kPreconditionsWebStateTimeoutInSeconds = 1.0;

}  // namespace link_to_text
