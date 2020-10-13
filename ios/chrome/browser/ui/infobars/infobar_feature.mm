// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_feature.h"

#include "components/infobars/core/infobar_feature.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kInfobarOverlayUI{"InfobarOverlayUI",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Feature enabled by default since it will always be checked along
// kIOSInfobarUIReboot, effectively working as a kill switch. Meaning that if
// kIOSInfobarUIReboot is not enabled this feature won't work.
const base::Feature kSaveCardInfobarMessagesUI{
    "SaveCardInfobarMessagesUI", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature enabled by default since it will always be guarded with
// |kIOSInfobarUIReboot|, meaning that if necessary,
// |kTranslateInfobarMessagesUI| can be used as a kill switch.
const base::Feature kTranslateInfobarMessagesUI{
    "TranslateInfobarMessagesUI", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kInfobarUIRebootOnlyiOS13{"InfobarUIRebootOnlyiOS13",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

bool IsInfobarUIRebootEnabled() {
  return base::FeatureList::IsEnabled(kIOSInfobarUIReboot);
}

bool IsInfobarOverlayUIEnabled() {
  return IsInfobarUIRebootEnabled() &&
         base::FeatureList::IsEnabled(kInfobarOverlayUI);
}

bool IsSaveCardInfobarMessagesUIEnabled() {
  return base::FeatureList::IsEnabled(kSaveCardInfobarMessagesUI) &&
         IsInfobarUIRebootEnabled();
}

bool IsTranslateInfobarMessagesUIEnabled() {
  return base::FeatureList::IsEnabled(kTranslateInfobarMessagesUI) &&
         IsInfobarUIRebootEnabled();
}
