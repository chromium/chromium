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

const base::Feature kBlockUniversalLinksInOffTheRecordMode{
    "BlockUniversalLinksInOffTheRecord", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kKeepsRenderProcessAlive{"KeepsRenderProcessAlive",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClearOldNavigationRecordsWorkaround{
    "ClearOldNavigationRecordsWorkaround", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnablePersistentDownloads{
    "EnablePersistentDownloads", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPreserveScrollViewProperties{
    "PreserveScrollViewProperties", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRecordSnapshotSize{"RecordSnapshotSize",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSetRequestAttribution{"SetRequestAttribution",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDefaultWebViewContextMenu{
    "DefaultWebViewContextMenu", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDisableNonHTMLScreenshotOnIOS15{
    "DisableNonHTMLScreenshotOnIOS15", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIOSSharedHighlightingColorChange{
    "IOSSharedHighlightingColorChange", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableNewDownloadAPI{"EnableNewDownloadAPI",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSynthesizedRestoreSession{
    "SynthesizedRestoreSession", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableUnrealizedWebStates{
    "EnableUnrealizedWebStates", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMediaPermissionsControl{"MediaPermissionsControl",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kEnableFullscreenAPI{
    "EnableFullscreenAPI", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kUseLoadSimulatedRequestForOfflinePage{
    "UseLoadSimulatedRequestForErrorPageNavigation",
    base::FEATURE_DISABLED_BY_DEFAULT};

bool UseWebViewNativeContextMenuWeb() {
  return base::FeatureList::IsEnabled(kDefaultWebViewContextMenu);
}

bool ShouldTakeScreenshotOnNonHTMLContent() {
  if (@available(iOS 15, *)) {
    return !base::FeatureList::IsEnabled(kDisableNonHTMLScreenshotOnIOS15);
  }
  return true;
}

bool IsNewDownloadAPIEnabled() {
  if (@available(iOS 15, *)) {
    return base::FeatureList::IsEnabled(kEnableNewDownloadAPI);
  }
  return false;
}

bool IsMediaPermissionsControlEnabled() {
  if (@available(iOS 15, *)) {
    return base::FeatureList::IsEnabled(kMediaPermissionsControl);
  }
  return false;
}

bool IsLoadSimulatedRequestAPIEnabled() {
  if (@available(iOS 15, *)) {
    return base::FeatureList::IsEnabled(kUseLoadSimulatedRequestForOfflinePage);
  }
  return false;
}

}  // namespace features
}  // namespace web
