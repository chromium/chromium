// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_FEATURE_LIBRARY_H_
#define MEDIA_LEARNING_COMMON_FEATURE_LIBRARY_H_

#include "base/component_export.h"
#include "media/learning/common/learning_task.h"

namespace media {
namespace learning {

// This class provides feature descriptions for common features provided by the
// learning framework.  When creating a LearningTask, one may choose to include
// these in the feature descriptions:
//
//   LearningTask my_task;
//   my_task.feature_descriptions.push_back(FeatureLibrary::NetworkType());
struct COMPONENT_EXPORT(LEARNING_COMMON) FeatureLibrary {
  // Common browser features
  // Current network connection type (wired, 3G, etc.).
  static LearningTask::ValueDescription NetworkType();

  // Is the device on battery power?
  static LearningTask::ValueDescription BatteryPower();

  // TODO(liberato): add CpuLoad, ConcurrentMediaPlayers, NetworkUsage, Battery.

  // Common renderer features
  // TODO(liberato): Add Element{Type, Path, Id, Name}, visibility, size, other
  // DOM structure features.
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_FEATURE_LIBRARY_H_
