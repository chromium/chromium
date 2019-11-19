// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/test_support_android.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/java/system/jni_headers/MojoTestRule_jni.h"

using base::android::JavaParamRef;

namespace {

struct TestEnvironment {
  TestEnvironment() {}

  base::ShadowingAtExitManager at_exit;
  base::SingleThreadTaskExecutor main_task_executor;
};

}  // namespace

namespace mojo {
namespace android {

static void JNI_MojoTestRule_InitCore(JNIEnv* env) {
  mojo::core::Init();
}

static void JNI_MojoTestRule_Init(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller) {
  base::InitAndroidTestMessageLoop();
}

static jlong JNI_MojoTestRule_SetupTestEnvironment(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return reinterpret_cast<intptr_t>(new TestEnvironment());
}

static void JNI_MojoTestRule_TearDownTestEnvironment(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong test_environment) {
  delete reinterpret_cast<TestEnvironment*>(test_environment);
}

static void JNI_MojoTestRule_RunLoop(JNIEnv* env,
                                     const JavaParamRef<jobject>& jcaller,
                                     jlong timeout_ms) {
  base::RunLoop run_loop;
  if (timeout_ms) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure(),
        base::TimeDelta::FromMilliseconds(timeout_ms));
    run_loop.Run();
  } else {
    run_loop.RunUntilIdle();
  }
}

}  // namespace android
}  // namespace mojo
