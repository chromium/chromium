// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_startup_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/browser/android/content_startup_flags.h"
#include "content/browser/browser_main_loop.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_main_dex_jni/BrowserStartupControllerImpl_jni.h"

using base::android::JavaParamRef;

namespace content {

void BrowserStartupComplete(int result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserStartupControllerImpl_browserStartupComplete(env, result);
}

void MinimalBrowserStartupComplete() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserStartupControllerImpl_minimalBrowserStartupComplete(env);
}

bool ShouldStartGpuProcessOnBrowserStartup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_BrowserStartupControllerImpl_shouldStartGpuProcessOnBrowserStartup(
      env);
}

static void JNI_BrowserStartupControllerImpl_SetCommandLineFlags(
    JNIEnv* env,
    jboolean single_process) {
  SetContentCommandLineFlags(static_cast<bool>(single_process));
}

static void JNI_BrowserStartupControllerImpl_FlushStartupTasks(JNIEnv* env) {
  BrowserMainLoop::GetInstance()->SynchronouslyFlushStartupTasks();
}

}  // namespace content
