// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/android/render_frame_host_test_ext.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/render_frame_host_android.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "ui/gfx/geometry/rect.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/test/android/content_test_jni/RenderFrameHostTestExt_jni.h"

using base::android::JavaParamRef;

namespace content {

namespace {

const void* const kRenderFrameHostTestExtKey = &kRenderFrameHostTestExtKey;

void OnExecuteJavaScriptResult(const base::android::JavaRef<jobject>& jcallback,
                               base::Value value) {
  std::string result;
  JSONStringValueSerializer serializer(&result);
  bool value_serialized = serializer.SerializeAndOmitBinaryValues(value);
  DCHECK(value_serialized);
  base::android::RunStringCallbackAndroid(jcallback, result);
}

}  // namespace

jlong JNI_RenderFrameHostTestExt_Init(JNIEnv* env,
                                      jlong render_frame_host_android_ptr) {
  RenderFrameHostAndroid* rfha =
      reinterpret_cast<RenderFrameHostAndroid*>(render_frame_host_android_ptr);
  auto* host = new RenderFrameHostTestExt(
      static_cast<RenderFrameHostImpl*>(rfha->render_frame_host()));
  return reinterpret_cast<intptr_t>(host);
}

RenderFrameHostTestExt::RenderFrameHostTestExt(RenderFrameHostImpl* rfhi)
    : render_frame_host_(rfhi) {
  render_frame_host_->SetUserData(kRenderFrameHostTestExtKey,
                                  base::WrapUnique(this));
}

void RenderFrameHostTestExt::ExecuteJavaScript(
    JNIEnv* env,
    const JavaParamRef<jstring>& jscript,
    const JavaParamRef<jobject>& jcallback,
    jboolean with_user_gesture) {
  std::u16string script(base::android::ConvertJavaStringToUTF16(env, jscript));
  auto callback = base::BindOnce(
      &OnExecuteJavaScriptResult,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback));
  if (with_user_gesture) {
    render_frame_host_->ExecuteJavaScriptWithUserGestureForTests(
        script, std::move(callback), ISOLATED_WORLD_ID_GLOBAL);
  } else {
    render_frame_host_->ExecuteJavaScriptForTests(script, std::move(callback),
                                                  ISOLATED_WORLD_ID_GLOBAL);
  }
}

void RenderFrameHostTestExt::UpdateVisualState(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  auto result_callback = base::BindOnce(
      &base::android::RunBooleanCallbackAndroid,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback));
  render_frame_host_->InsertVisualStateCallback(std::move(result_callback));
}

void RenderFrameHostTestExt::NotifyVirtualKeyboardOverlayRect(JNIEnv* env,
                                                              jint x,
                                                              jint y,
                                                              jint width,
                                                              jint height) {
  gfx::Size size(width, height);
  gfx::Point origin(x, y);
  render_frame_host_->GetPage().NotifyVirtualKeyboardOverlayRect(
      gfx::Rect(origin, size));
}

}  // namespace content
