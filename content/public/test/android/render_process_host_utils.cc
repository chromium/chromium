// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/test/browser_test_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/test/android/content_test_jni/RenderProcessHostUtils_jni.h"

namespace content {

jint JNI_RenderProcessHostUtils_GetCurrentRenderProcessCount(JNIEnv* env) {
  return RenderProcessHost::GetCurrentRenderProcessCountForTesting();
}
jint JNI_RenderProcessHostUtils_GetSpareRenderProcessHostCount(JNIEnv* env) {
  return SpareRenderProcessHostManager::Get().GetSpares().size();
}
jint JNI_RenderProcessHostUtils_GetSpareRenderBindingState(JNIEnv* env) {
  RenderProcessHost* spare =
      SpareRenderProcessHostManager::Get().GetSpares()[0];
  return static_cast<jint>(spare->GetEffectiveChildBindingState());
}
jboolean JNI_RenderProcessHostUtils_IsSpareRenderReady(JNIEnv* env) {
  RenderProcessHost* spare =
      SpareRenderProcessHostManager::Get().GetSpares()[0];
  return spare->IsReady();
}
}  // namespace content
