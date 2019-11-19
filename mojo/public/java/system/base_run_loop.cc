// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/public/java/system/system_impl_java_jni_headers/BaseRunLoop_jni.h"

using base::android::JavaParamRef;

namespace mojo {
namespace android {

static jlong JNI_BaseRunLoop_CreateBaseRunLoop(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  base::SingleThreadTaskExecutor* task_executor =
      new base::SingleThreadTaskExecutor;
  return reinterpret_cast<uintptr_t>(task_executor);
}

static void JNI_BaseRunLoop_Run(JNIEnv* env,
                                const JavaParamRef<jobject>& jcaller) {
  base::RunLoop().Run();
}

static void JNI_BaseRunLoop_RunUntilIdle(JNIEnv* env,
                                         const JavaParamRef<jobject>& jcaller) {
  base::RunLoop().RunUntilIdle();
}

static void JNI_BaseRunLoop_Quit(JNIEnv* env,
                                 const JavaParamRef<jobject>& jcaller) {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

static void RunJavaRunnable(
    const base::android::ScopedJavaGlobalRef<jobject>& runnable_ref) {
  Java_BaseRunLoop_runRunnable(base::android::AttachCurrentThread(),
                               runnable_ref);
}

static void JNI_BaseRunLoop_PostDelayedTask(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong runLoopID,
    const JavaParamRef<jobject>& runnable,
    jlong delay) {
  base::android::ScopedJavaGlobalRef<jobject> runnable_ref;
  // ScopedJavaGlobalRef do not hold onto the env reference, so it is safe to
  // use it across threads. |RunJavaRunnable| will acquire a new JNIEnv before
  // running the Runnable.
  runnable_ref.Reset(env, runnable);
  reinterpret_cast<base::SingleThreadTaskExecutor*>(runLoopID)
      ->task_runner()
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(&RunJavaRunnable, runnable_ref),
                        base::TimeDelta::FromMicroseconds(delay));
}

static void JNI_BaseRunLoop_DeleteMessageLoop(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong runLoopID) {
  base::SingleThreadTaskExecutor* task_executor =
      reinterpret_cast<base::SingleThreadTaskExecutor*>(runLoopID);
  delete task_executor;
}

}  // namespace android
}  // namespace mojo
