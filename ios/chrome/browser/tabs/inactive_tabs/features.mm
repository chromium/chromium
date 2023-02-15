// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/inactive_tabs/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kTabInactivityThreshold,
             "TabInactivityThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kTabInactivityThresholdParameterName[] = "default";
const char kTabInactivityThresholdOneWeekParam[] =
    "tab-inactivity-threshold-one-week";
const char kTabInactivityThresholdTwoWeeksParam[] =
    "tab-inactivity-threshold-two-weeks";
const char kTabInactivityThresholdThreeWeeksParam[] =
    "tab-inactivity-threshold-three-weeks";

bool IsInactiveTabsEnabled() {
  bool isIPhoneIdiom =
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
  return isIPhoneIdiom && base::FeatureList::IsEnabled(kTabInactivityThreshold);
}

base::TimeDelta TabInactivityThreshold() {
  DCHECK(IsInactiveTabsEnabled());
  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      kTabInactivityThreshold, kTabInactivityThresholdParameterName);
  if (featureParam == kTabInactivityThresholdOneWeekParam) {
    return base::Days(8);
  } else if (featureParam == kTabInactivityThresholdTwoWeeksParam) {
    return base::Days(15);
  } else if (featureParam == kTabInactivityThresholdThreeWeeksParam) {
    return base::Days(22);
  }
  return base::Days(15);
}

std::u16string TabInactivityThresholdDisplayString() {
  DCHECK(IsInactiveTabsEnabled());
  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      kTabInactivityThreshold, kTabInactivityThresholdParameterName);
  if (featureParam == kTabInactivityThresholdOneWeekParam) {
    return u"7";
  } else if (featureParam == kTabInactivityThresholdTwoWeeksParam) {
    return u"14";
  } else if (featureParam == kTabInactivityThresholdThreeWeeksParam) {
    return u"21";
  }
  return u"14";
}

BASE_FEATURE(kShowInactiveTabsCount,
             "ShowInactiveTabsCount",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsShowInactiveTabsCountEnabled() {
  DCHECK(IsInactiveTabsEnabled());
  return base::FeatureList::IsEnabled(kShowInactiveTabsCount);
}
