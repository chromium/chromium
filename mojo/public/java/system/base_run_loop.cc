// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/jni_zero/common_apis.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "mojo/public/java/system/system_impl_java_jni_headers/BaseRunLoop_jni.h"

using base::android::JavaRef;

namespace mojo {
namespace android {

static int64_t JNI_BaseRunLoop_CreateBaseRunLoop(JNIEnv* env) {
  base::SingleThreadTaskExecutor* task_executor =
      new base::SingleThreadTaskExecutor;
  return reinterpret_cast<uintptr_t>(task_executor);
}

static void JNI_BaseRunLoop_Run(JNIEnv* env) {
  base::RunLoop().Run();
}

static void JNI_BaseRunLoop_RunUntilIdle(JNIEnv* env) {
  base::RunLoop().RunUntilIdle();
}

static void JNI_BaseRunLoop_PostDelayedTask(JNIEnv* env,
                                            int64_t runLoopID,
                                            base::OnceClosure&& runnable,
                                            int64_t delay) {
  reinterpret_cast<base::SingleThreadTaskExecutor*>(runLoopID)
      ->task_runner()
      ->PostDelayedTask(FROM_HERE, std::move(runnable),
                        base::Microseconds(delay));
}

static void JNI_BaseRunLoop_DeleteMessageLoop(JNIEnv* env, int64_t runLoopID) {
  base::SingleThreadTaskExecutor* task_executor =
      reinterpret_cast<base::SingleThreadTaskExecutor*>(runLoopID);
  delete task_executor;
}

}  // namespace android
}  // namespace mojo

DEFINE_JNI(BaseRunLoop)
