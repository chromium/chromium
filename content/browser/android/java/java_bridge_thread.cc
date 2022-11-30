// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/java_bridge_thread.h"

#include "base/lazy_instance.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#error "JavaBridge only supports OS_ANDROID"
#endif

namespace content {

namespace {

base::LazyInstance<JavaBridgeThread>::DestructorAtExit g_background_thread =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

JavaBridgeThread::JavaBridgeThread()
    : base::android::JavaHandlerThread("JavaBridge") {
  Start();
}

JavaBridgeThread::~JavaBridgeThread() {
  Stop();
}

// static
bool JavaBridgeThread::CurrentlyOn() {
  return g_background_thread.Get().task_runner()->BelongsToCurrentThread();
}

// static
scoped_refptr<base::SingleThreadTaskRunner> JavaBridgeThread::GetTaskRunner() {
  return g_background_thread.Get().task_runner();
}

}  // namespace content
