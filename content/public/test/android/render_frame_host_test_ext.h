// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ANDROID_RENDER_FRAME_HOST_TEST_EXT_H_
#define CONTENT_PUBLIC_TEST_ANDROID_RENDER_FRAME_HOST_TEST_EXT_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace content {

class RenderFrameHostImpl;

class RenderFrameHostTestExt : public base::SupportsUserData::Data {
 public:
  explicit RenderFrameHostTestExt(RenderFrameHostImpl* rfhi);

  void ExecuteJavaScript(JNIEnv* env,
                         const base::android::JavaRef<jstring>& jscript,
                         const base::android::JavaRef<jobject>& jcallback,
                         bool with_user_gesture);
  // This calls InsertVisualStateCallback(). See it for details on the return
  // value.
  void UpdateVisualState(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jcallback);

  void NotifyVirtualKeyboardOverlayRect(JNIEnv* env,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height);

 private:
  const raw_ptr<RenderFrameHostImpl> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ANDROID_RENDER_FRAME_HOST_TEST_EXT_H_
