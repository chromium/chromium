// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/launcher_thread.h"

#include "content/public/android/content_jni_headers/LauncherThread_jni.h"

namespace content {
namespace android {

namespace {
base::LazyInstance<LauncherThread>::Leaky g_launcher_thread;
}

scoped_refptr<base::SingleThreadTaskRunner> LauncherThread::GetTaskRunner() {
  return g_launcher_thread.Get().java_handler_thread_.task_runner();
}

LauncherThread::LauncherThread()
    : java_handler_thread_(nullptr,
                           Java_LauncherThread_getHandlerThread(
                               base::android::AttachCurrentThread())) {
  java_handler_thread_.Start();
}

LauncherThread::~LauncherThread() {}

}  // namespace android
}  // namespace content
