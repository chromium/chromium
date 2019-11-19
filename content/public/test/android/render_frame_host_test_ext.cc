// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/android/render_frame_host_test_ext.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "content/browser/frame_host/render_frame_host_android.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
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
                                      const JavaParamRef<jobject>& obj,
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
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jscript,
    const JavaParamRef<jobject>& jcallback) {
  base::string16 script(ConvertJavaStringToUTF16(env, jscript));
  auto callback = base::BindOnce(
      &OnExecuteJavaScriptResult,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback));
  render_frame_host_->ExecuteJavaScriptForTests(script, std::move(callback));
}

}  // namespace content
