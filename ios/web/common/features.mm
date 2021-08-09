// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/features.h"

#include "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace features {

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

const base::Feature kUseDefaultUserAgentInWebClient{
    "UseDefaultUserAgentInWebClient", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPreserveScrollViewProperties{
    "PreserveScrollViewProperties", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIOSLegacyTLSInterstitial{"IOSLegacyTLSInterstitial",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRecordSnapshotSize{"RecordSnapshotSize",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebViewNativeContextMenu{"WebViewNativeContextMenu",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWebViewNativeContextMenuPhase2{
    "WebViewNativeContextMenuPhase2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebViewNativeContextMenuPhase3{
    "WebViewNativeContextMenuPhase3", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultWebViewContextMenu{
    "DefaultWebViewContextMenu", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDisableNonHTMLScreenshotOnIOS15{
    "DisableNonHTMLScreenshotOnIOS15", base::FEATURE_ENABLED_BY_DEFAULT};

bool UseWebClientDefaultUserAgent() {
  if (@available(iOS 13, *)) {
    return base::FeatureList::IsEnabled(kUseDefaultUserAgentInWebClient);
  }
  return false;
}

bool UseWebViewNativeContextMenuWeb() {
  return base::FeatureList::IsEnabled(kDefaultWebViewContextMenu);
}

bool UseWebViewNativeContextMenuSystem() {
  return base::FeatureList::IsEnabled(kWebViewNativeContextMenu) ||
         base::FeatureList::IsEnabled(kWebViewNativeContextMenuPhase2) ||
         base::FeatureList::IsEnabled(kWebViewNativeContextMenuPhase3);
}

bool ShouldTakeScreenshotOnNonHTMLContent() {
  if (@available(iOS 15, *)) {
    return !base::FeatureList::IsEnabled(kDisableNonHTMLScreenshotOnIOS15);
  }
  return true;
}

const base::Feature kIOSSharedHighlightingColorChange{
    "IOSSharedHighlightingColorChange", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCreatePendingItemForPostFormSubmission{
    "CreatePendingItemForPostFormSubmission",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace web
