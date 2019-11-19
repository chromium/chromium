// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace features {

const base::Feature kIgnoresViewportScaleLimits{
    "IgnoresViewportScaleLimits", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSlimNavigationManager{"SlimNavigationManager",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCrashOnUnexpectedURLChange{
    "CrashOnUnexpectedURLChange", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHistoryClobberWorkaround{
    "WKWebViewHistoryClobberWorkaround", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBlockUniversalLinksInOffTheRecordMode{
    "BlockUniversalLinksInOffTheRecord", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kKeepsRenderProcessAlive{"KeepsRenderProcessAlive",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClearOldNavigationRecordsWorkaround{
    "ClearOldNavigationRecordsWorkaround", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSSLCommittedInterstitials{
    "SSLCommittedInterstitials", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseWKWebViewLoading{"UseWKWebViewLoading",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLogLoadStartedInDidStartNavigation{
    "LogLoadStartedInDidStartNavigation", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnablePersistentDownloads{
    "EnablePersistentDownloads", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseJSForErrorPage{"UseJSForErrorPage",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// The feature kUseWKWebViewLoading will change the CPM if
// kLogLoadStartedInDidStartNavigation is not enabled, so
// kLogLoadStartedInDidStartNavigation is required. The feature flag
// kUseWKWebViewLoading itself is also required to make sure CPM is not changed
// even with kLogLoadStartedInDidStartNavigation enabled. The flag
// kSlimNavigationManager is used here because we only want to fix test failure
// for slim-nav since it's going to be shipped soon.
bool UseWKWebViewLoading() {
  return base::FeatureList::IsEnabled(web::features::kSlimNavigationManager) &&
         base::FeatureList::IsEnabled(web::features::kUseWKWebViewLoading) &&
         base::FeatureList::IsEnabled(
             web::features::kLogLoadStartedInDidStartNavigation);
}

}  // namespace features
}  // namespace web
