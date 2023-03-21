// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/bring_android_tabs/bring_android_tabs_metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace bring_android_tabs {

extern const char kTabCountHistogramName[] = "IOS.BringAndroidTabs.TabCount";

extern const char kPromptActionHistogramName[] =
    "IOS.BringAndroidTabs.ActionOnPrompt";

extern const char kTabListActionHistogramName[] =
    "IOS.BringAndroidTabs.ActionOnTabsList";

extern const char kDeselectedTabCountHistogramName[] =
    "IOS.BringAndroidTabs.DeselectedTabCount";

extern const char kPromptAttemptStatusHistogramName[] =
    "IOS.BringAndroidTabs.PromptTabsStatus";

}  // namespace bring_android_tabs
