// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/post_restore/metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace post_restore_default_browser {

const char kPromptActionHistogramName[] =
    "IOS.PostRestoreDefaultBrowser.ActionOnPrompt";

const char kPromptDisplayedUserActionName[] =
    "IOS.PostRestoreDefaultBrowser.Displayed";

}  // namespace post_restore_default_browser
