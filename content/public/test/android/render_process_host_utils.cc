// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "content/public/browser/render_process_host.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/test/android/content_test_jni/RenderProcessHostUtils_jni.h"

namespace content {

jint JNI_RenderProcessHostUtils_GetCurrentRenderProcessCount(JNIEnv* env) {
  return RenderProcessHost::GetCurrentRenderProcessCountForTesting();
}

}  // namespace content
