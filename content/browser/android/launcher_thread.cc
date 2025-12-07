// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/launcher_thread.h"

#include "base/task/single_thread_task_runner.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/LauncherThread_jni.h"

namespace content {
namespace android {

namespace {
LauncherThread& GetInstance() {
  static base::NoDestructor<LauncherThread> launcher_thread;
  return *launcher_thread;
}
}

scoped_refptr<base::SingleThreadTaskRunner> LauncherThread::GetTaskRunner() {
  return GetInstance().java_handler_thread_.task_runner();
}

LauncherThread::LauncherThread()
    : java_handler_thread_(nullptr,
                           Java_LauncherThread_getHandlerThread(
                               jni_zero::AttachCurrentThread())) {
  java_handler_thread_.Start();
}

LauncherThread::~LauncherThread() = default;

}  // namespace android
}  // namespace content

DEFINE_JNI(LauncherThread)
