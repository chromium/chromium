// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_

#include <memory>
#include <vector>

#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
struct Config;
class ModelProvider;

// Returns a Config created from the finch feature params.
std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig();

// Implementation of FieldTrialRegister that uses synthetic field trials to
// record segmentation groups.
class IOSFieldTrialRegisterImpl : public FieldTrialRegister {
 public:
  IOSFieldTrialRegisterImpl();
  ~IOSFieldTrialRegisterImpl() override;
  IOSFieldTrialRegisterImpl(const IOSFieldTrialRegisterImpl&) = delete;
  IOSFieldTrialRegisterImpl& operator=(const IOSFieldTrialRegisterImpl&) =
      delete;

  // FieldTrialRegister:
  void RegisterFieldTrial(base::StringPiece trial_name,
                          base::StringPiece group_name) override;

  void RegisterSubsegmentFieldTrialIfNeeded(base::StringPiece trial_name,
                                            proto::SegmentId segment_id,
                                            int subsegment_rank) override;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_CONFIG_H_
