// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_test_utils.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/segment_selection_result.h"

namespace segmentation_platform {

namespace test {

FakeDeviceSwitcherResultDispatcher::FakeDeviceSwitcherResultDispatcher(
    segmentation_platform::SegmentationPlatformService* segmentation_service,
    syncer::DeviceInfoTracker* device_info_tracker,
    PrefService* prefs,
    segmentation_platform::FieldTrialRegister* field_trial_register)
    : segmentation_platform::DeviceSwitcherResultDispatcher(
          segmentation_service,
          device_info_tracker,
          prefs,
          field_trial_register),
      classification_result_(
          segmentation_platform::PredictionStatus::kNotReady) {}

FakeDeviceSwitcherResultDispatcher::~FakeDeviceSwitcherResultDispatcher() =
    default;

void FakeDeviceSwitcherResultDispatcher::WaitForClassificationResult(
    base::TimeDelta timeout,
    segmentation_platform::ClassificationResultCallback callback) {
  std::vector<std::string> ordered_labels;
  if (segment_label_ ==
      segmentation_platform::DefaultBrowserUserSegment::kDesktopUser) {
    ordered_labels = {
        segmentation_platform::DeviceSwitcherModel::kOtherLabel,
        segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
        segmentation_platform::DeviceSwitcherModel::kDesktopLabel,
    };
  } else if (segment_label_ ==
             segmentation_platform::DefaultBrowserUserSegment::
                 kAndroidSwitcher) {
    ordered_labels = {
        segmentation_platform::DeviceSwitcherModel::kOtherLabel,
        segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
    };
  } else {
    ordered_labels = {segmentation_platform::DeviceSwitcherModel::kOtherLabel};
  }

  segmentation_platform::ClassificationResult modified_result(
      classification_result_.status);
  modified_result.ordered_labels = ordered_labels;

  std::move(callback).Run(std::move(modified_result));
}

void FakeDeviceSwitcherResultDispatcher::SetSegmentLabel(
    segmentation_platform::DefaultBrowserUserSegment label) {
  segment_label_ = label;
}

void FakeDeviceSwitcherResultDispatcher::SetPredictionStatus(
    segmentation_platform::PredictionStatus status) {
  classification_result_.status = status;
}

}  // namespace test

}  // namespace segmentation_platform
