// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_action_policy_util.h"

#include "base/feature_list.h"
#include "ios/web/common/features.h"
#include "ios/web/navigation/block_universal_links_buildflags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

const WKNavigationActionPolicy
    kNavigationActionPolicyAllowAndBlockUniversalLinks =
        static_cast<WKNavigationActionPolicy>(WKNavigationActionPolicyAllow +
                                              2);

WKNavigationActionPolicy GetAllowNavigationActionPolicy(
    bool block_universal_links) {
  // When both the |block_universal_links_in_off_the_record| gn arg and the
  // |web::features::kBlockUniversalLinksInOffTheRecordMode| feature flag are
  // enabled, the returned value will block opening native applications if
  // |block_universal_links| is true to prevent sharing off the record state or
  // opening an app unexpectedly during restore.
#if BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)
  bool block_universal_links_enabled = base::FeatureList::IsEnabled(
      web::features::kBlockUniversalLinksInOffTheRecordMode);
  if (block_universal_links && block_universal_links_enabled) {
    return kNavigationActionPolicyAllowAndBlockUniversalLinks;
  }
#endif  // BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)

  return WKNavigationActionPolicyAllow;
}

}  // namespace web
