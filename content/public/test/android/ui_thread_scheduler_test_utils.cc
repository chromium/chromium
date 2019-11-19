// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/test/android/content_test_jni/UiThreadSchedulerTestUtils_jni.h"

namespace content {

void JNI_UiThreadSchedulerTestUtils_PostBrowserMainLoopStartupTasks(
    JNIEnv* env,
    jboolean enabled) {
  BrowserMainLoop::EnableStartupTasks(enabled);
}

}  // namespace content
