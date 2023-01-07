// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kEnableOpenInDownload,
             "EnableFREDefaultBrowserScreenTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kOpenInDownloadInShareButtonParam[] = "variant_with_openin_download";
const char kOpenInDownloadWithWKDownloadParam[] = "variant_with_wkdownload";
const char kOpenInDownloadWithV2Param[] = "variant_with_v2";

bool IsOpenInDownloadInShareButton() {
  if (@available(iOS 14.5, *)) {
    if (base::FeatureList::IsEnabled(kEnableOpenInDownload)) {
      return base::GetFieldTrialParamByFeatureAsBool(
          kEnableOpenInDownload, kOpenInDownloadInShareButtonParam, false);
    }
  }
  return false;
}

bool IsOpenInDownloadWithWKDownload() {
  if (@available(iOS 14.5, *)) {
    if (base::FeatureList::IsEnabled(kEnableOpenInDownload)) {
      return base::GetFieldTrialParamByFeatureAsBool(
          kEnableOpenInDownload, kOpenInDownloadWithWKDownloadParam, false);
    }
  }
  return false;
}

bool IsOpenInDownloadWithV2() {
  if (@available(iOS 14.5, *)) {
    if (base::FeatureList::IsEnabled(kEnableOpenInDownload)) {
      return base::GetFieldTrialParamByFeatureAsBool(
          kEnableOpenInDownload, kOpenInDownloadWithV2Param, false);
    }
  }
  return false;
}

bool IsOpenInNewDownloadEnabled() {
  return IsOpenInDownloadWithV2() || IsOpenInDownloadWithWKDownload();
}

bool IsOpenInActivitiesInShareButtonEnabled() {
  return IsOpenInDownloadWithV2() || IsOpenInDownloadWithWKDownload() ||
         IsOpenInDownloadInShareButton();
}
