// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This flag should be turned off by default until gmail.com can detect the
// user has been signed out.
// See: http://crbug.com/939508.
const base::Feature kUseNSURLSessionForGaiaSigninRequests{
    "UseNSURLSessionForGaiaSigninRequests", base::FEATURE_DISABLED_BY_DEFAULT};
