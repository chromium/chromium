// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kNativeFindInPage,
             "NativeFindInPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kNativeFindInPageParameterName[] = "variant";

const char kNativeFindInPageWithChromeFindBarParam[] =
    "variant_with_chrome_find_bar";
