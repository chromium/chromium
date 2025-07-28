// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_

#include "device/vr/openxr/openxr_hit_test_manager.h"
#include "device/vr/openxr/openxr_plane_manager.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// The OpenXRSceneUnderstandingManager is responsible for managing various
// delegates that need to interact with each other. These delegates sometimes
// rely on separate extensions that are nevertheless part of a complete "system"
// where the entire system of extensions can talk to each other, but not really
// to other types of extensions. The goal of this class then is to help ensure
// that any delegates that are part of such systems are created together, and to
// help with dependency injection and lifecycle management of these delegates.
class OpenXRSceneUnderstandingManager {
 public:
  OpenXRSceneUnderstandingManager();
  virtual ~OpenXRSceneUnderstandingManager();

  virtual OpenXrPlaneManager* GetPlaneManager() = 0;
  virtual OpenXrHitTestManager* GetHitTestManager() = 0;
};

}  // namespace device
#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_
