// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_handler_factories.h"

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "device/vr/openxr/fb/openxr_hand_tracker_fb.h"
#include "device/vr/openxr/msft/openxr_anchor_manager_msft.h"
#include "device/vr/openxr/msft/openxr_scene_understanding_manager_msft.h"
#include "device/vr/openxr/msft/openxr_unbounded_space_provider_msft.h"
#include "device/vr/openxr/openxr_hand_tracker.h"
#include "device/vr/openxr/openxr_stage_bounds_provider_basic.h"

#if BUILDFLAG(IS_ANDROID)
#include "device/vr/openxr/android/openxr_anchor_manager_android.h"
#include "device/vr/openxr/android/openxr_depth_sensor_android.h"
#include "device/vr/openxr/android/openxr_light_estimator_android.h"
#include "device/vr/openxr/android/openxr_scene_understanding_manager_android.h"
#include "device/vr/openxr/android/openxr_stage_bounds_provider_android.h"
#include "device/vr/openxr/android/openxr_unbounded_space_provider_android.h"
#endif

namespace device {
const std::vector<OpenXrExtensionHandlerFactory*>&
GetExtensionHandlerFactories() {
  static base::NoDestructor<std::vector<OpenXrExtensionHandlerFactory*>>
      kFactories{std::vector<OpenXrExtensionHandlerFactory*>{
  // List platform-specific extensions first as they should generally be
  // preferred on the platforms that they are supported for.
#if BUILDFLAG(IS_ANDROID)
          new OpenXrStageBoundsProviderAndroidFactory(),

          new OpenXrUnboundedSpaceProviderAndroidFactory(),

          new OpenXrSceneUnderstandingManagerAndroidFactory(),

          new OpenXrAnchorManagerAndroidFactory(),

          new OpenXrLightEstimatorAndroidFactory(),

          new OpenXrDepthSensorAndroidFactory(),
#endif

          // List the hand trackers that can supply hand interaction data (e.g.
          // parsed pinches) first, as otherwise they won't be created. Their
          // parsed interaction data will only be queried if no supported
          // interaction profile can be enabled, and otherwise they should still
          // be able to supply any hand/joint data just as well as the default
          // hand tracker (which can essentially only provide joint data).
          new OpenXrHandTrackerFbFactory(),
          new OpenXrHandTrackerFactory(),

          new OpenXrStageBoundsProviderBasicFactory(),

          new OpenXrUnboundedSpaceProviderMsftFactory(),

          new OpenXrSceneUnderstandingManagerMsftFactory(),

          new OpenXrAnchorManagerMsftFactory(),
      }};

  return *kFactories;
}
}  // namespace device
