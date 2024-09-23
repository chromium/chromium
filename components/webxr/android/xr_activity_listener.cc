// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/xr_activity_listener.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "components/webxr/android/webxr_utils.h"
#include "device/vr/android/xr_activity_state_handler.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webxr/android/xr_jni_headers/XrActivityListener_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace webxr {

XrActivityListenerFactory::XrActivityListenerFactory() = default;
XrActivityListenerFactory::~XrActivityListenerFactory() = default;
std::unique_ptr<device::XrActivityStateHandler>
XrActivityListenerFactory::Create(int render_process_id, int render_frame_id) {
  return std::make_unique<XrActivityListener>(render_process_id,
                                              render_frame_id);
}

XrActivityListener::XrActivityListener(int render_process_id,
                                       int render_frame_id) {
  JNIEnv* env = AttachCurrentThread();
  if (!env) {
    return;
  }
  ScopedJavaLocalRef<jobject> j_xr_activity_listener =
      Java_XrActivityListener_Constructor(
          env, (jlong)this,
          webxr::GetJavaWebContents(render_process_id, render_frame_id));
  if (j_xr_activity_listener.is_null()) {
    return;
  }
  j_xr_activity_listener_.Reset(j_xr_activity_listener);
}

XrActivityListener::~XrActivityListener() {
  DVLOG(1) << __func__ << ": java_obj=" << !j_xr_activity_listener_.is_null();
  if (!j_xr_activity_listener_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_XrActivityListener_onNativeDestroy(env, j_xr_activity_listener_);
  }
}

void XrActivityListener::SetResumedHandler(
    base::RepeatingClosure resumed_handler) {
  on_activity_resumed_handler_ = std::move(resumed_handler);
}

void XrActivityListener::OnActivityResumed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DVLOG(1) << __func__ << ": has handler?=" << !!on_activity_resumed_handler_;
  if (on_activity_resumed_handler_) {
    on_activity_resumed_handler_.Run();
  }
}
}  // namespace webxr
