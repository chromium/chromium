// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_XR_ACTIVITY_LISTENER_H_
#define COMPONENTS_WEBXR_ANDROID_XR_ACTIVITY_LISTENER_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "device/vr/android/xr_activity_state_handler.h"

namespace webxr {

// This is the native equivalent of XrActivityListener.java. Creating this class
// will create the corresponding Java object which will subscribe to Activity
// lifecycle events, and then forward the calls down to this class. This class
// will then forward on the calls to any handler that it has registered. It is
// intended to be owned by and have it's registrations managed by a single
// class and not to have multiple listeners.
class XrActivityListener : public device::XrActivityStateHandler {
 public:
  explicit XrActivityListener(int render_process_id, int render_frame_id);
  ~XrActivityListener() override;

  // XrActivityStateHandler
  void SetResumedHandler(base::RepeatingClosure resumed_handler) override;

  // XrActivityListener JNI interface.
  void OnActivityResumed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_xr_activity_listener_;

  base::RepeatingClosure on_activity_resumed_handler_;
};

class XrActivityListenerFactory : public device::XrActivityStateHandlerFactory {
 public:
  XrActivityListenerFactory();
  ~XrActivityListenerFactory() override;
  std::unique_ptr<device::XrActivityStateHandler> Create(
      int render_process_id,
      int render_frame_id) override;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_XR_ACTIVITY_LISTENER_H_
