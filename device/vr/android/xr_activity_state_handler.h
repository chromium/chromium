// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_XR_ACTIVITY_STATE_HANDLER_H_
#define DEVICE_VR_ANDROID_XR_ACTIVITY_STATE_HANDLER_H_

#include <memory>

#include "base/functional/callback_forward.h"

namespace device {

// The purpose of this interface is to allow for dependency injection to allow
// //device code to listen to calls to the ActivityLifecycleCallbacks interface,
// which has to be implemented in //components, because it needs to access the
// WebContents and Activity objects, which cannot be used in //device.
//  Unfortunately, the implementation in //components must have the same name as
// the java class for the jni headers to build properly; so we're left to come
// up with a slightly different name here.
class XrActivityStateHandler {
 public:
  virtual ~XrActivityStateHandler() = default;
  virtual void SetResumedHandler(base::RepeatingClosure resumed_handler) = 0;
};

class XrActivityStateHandlerFactory {
 public:
  virtual ~XrActivityStateHandlerFactory() = default;
  virtual std::unique_ptr<XrActivityStateHandler> Create(
      int render_process_id,
      int render_frame_id) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_XR_ACTIVITY_STATE_HANDLER_H_
