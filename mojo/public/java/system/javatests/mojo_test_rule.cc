// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_support_android.h"
#include "mojo/core/embedder/embedder.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "mojo/public/java/system/test_support_jni/MojoTestRule_jni.h"

using base::android::JavaParamRef;

namespace {

struct TestEnvironment {
  base::ShadowingAtExitManager at_exit;
  base::SingleThreadTaskExecutor main_task_executor;
  std::optional<base::RunLoop> run_loop;
};

}  // namespace

namespace mojo {
namespace android {

static void JNI_MojoTestRule_InitCore(JNIEnv* env) {
  mojo::core::Init();
}

static void JNI_MojoTestRule_Init(JNIEnv* env) {
  base::InitAndroidTestMessageLoop();
}

static jlong JNI_MojoTestRule_SetupTestEnvironment(JNIEnv* env) {
  return reinterpret_cast<jlong>(new TestEnvironment());
}

static void JNI_MojoTestRule_TearDownTestEnvironment(
    JNIEnv* env,
    jlong raw_test_environment) {
  delete reinterpret_cast<TestEnvironment*>(raw_test_environment);
}

static void JNI_MojoTestRule_QuitLoop(JNIEnv* env, jlong raw_test_environment) {
  auto* test_environment =
      reinterpret_cast<TestEnvironment*>(raw_test_environment);
  test_environment->run_loop->Quit();
}

static void JNI_MojoTestRule_RunLoop(JNIEnv* env,
                                     jlong raw_test_environment,
                                     jlong timeout_ms) {
  auto& run_loop = reinterpret_cast<TestEnvironment*>(raw_test_environment)
                       ->run_loop.emplace();
  if (timeout_ms == 0) {
    run_loop.RunUntilIdle();
  } else if (timeout_ms < 0) {
    run_loop.Run();
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure(),
        base::Milliseconds(timeout_ms));
    run_loop.Run();
  }
}

}  // namespace android
}  // namespace mojo
