// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace features {

const base::Feature kReduceSessionSize{"ReduceSessionSize",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

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

const base::Feature kEnablePersistentDownloads{
    "EnablePersistentDownloads", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseJSForErrorPage{"UseJSForErrorPage",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseDefaultUserAgentInWebClient{
    "UseDefaultUserAgentInWebClient", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPreserveScrollViewProperties{
    "PreserveScrollViewProperties", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kScrollToTextIOS{"ScrollToTextIOS",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIOSLegacyTLSInterstitial{"IOSLegacyTLSInterstitial",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRecordSnapshotSize{"RecordSnapshotSize",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRestoreSessionFromCache{"RestoreSessionFromCache",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebViewNativeContextMenu{
    "WebViewNativeContextMenu", base::FEATURE_DISABLED_BY_DEFAULT};

const char kWebViewNativeContextMenuName[] = "type";
const char kWebViewNativeContextMenuParameterSystem[] = "system";
const char kWebViewNativeContextMenuParameterWeb[] = "web";

bool UseWebClientDefaultUserAgent() {
  if (@available(iOS 13, *)) {
    return base::FeatureList::IsEnabled(kUseDefaultUserAgentInWebClient);
  }
  return false;
}

bool UseWebViewNativeContextMenuWeb() {
  if (@available(iOS 13, *)) {
    if (!base::FeatureList::IsEnabled(kWebViewNativeContextMenu))
      return false;
    std::string field_trial_param = base::GetFieldTrialParamValueByFeature(
        kWebViewNativeContextMenu, kWebViewNativeContextMenuName);
    return field_trial_param == kWebViewNativeContextMenuParameterWeb;
  }
  return false;
}

bool UseWebViewNativeContextMenuSystem() {
  if (@available(iOS 13, *)) {
    if (!base::FeatureList::IsEnabled(kWebViewNativeContextMenu))
      return false;
    std::string field_trial_param = base::GetFieldTrialParamValueByFeature(
        kWebViewNativeContextMenu, kWebViewNativeContextMenuName);
    return field_trial_param == kWebViewNativeContextMenuParameterSystem;
  }
  return false;
}

const base::Feature kIOSSharedHighlightingColorChange{
    "IOSSharedHighlightingColorChange", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace web
