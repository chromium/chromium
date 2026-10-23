// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class sets up the environment for running the native tests inside an
// android application. It outputs (to a fifo) markers identifying the
// START/PASSED/CRASH of the test suite, FAILURE/SUCCESS of individual tests,
// etc.
// These markers are read by the test runner script to generate test results.
// It installs signal handlers to detect crashes.

#include "testing/android/native_test/native_browser_test_support.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "testing/android/native_test/native_browser_test_jni/NativeBrowserTest_jni.h"

namespace testing {
namespace android {

namespace {

// Java calls to set this true when async startup tasks are done for browser
// tests.
bool g_java_startup_tasks_complete = false;
bool g_activity_teardown_started = false;
bool g_activity_teardown_complete = false;

base::OnceClosure& GetJavaStartupTasksCompleteCallback() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

base::OnceClosure& GetActivityTeardownCompleteCallback() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

}  // namespace

static void JNI_NativeBrowserTest_JavaStartupTasksCompleteForBrowserTests(
    JNIEnv* env) {
  g_java_startup_tasks_complete = true;
  if (auto& callback = GetJavaStartupTasksCompleteCallback()) {
    std::move(callback).Run();
  }
}

static void JNI_NativeBrowserTest_ActivityTeardownCompleteForBrowserTests(
    JNIEnv* env) {
  g_activity_teardown_complete = true;
  if (auto& callback = GetActivityTeardownCompleteCallback()) {
    std::move(callback).Run();
  }
}

void SetJavaStartupTasksCompleteCallbackForBrowserTests(
    base::OnceClosure callback) {
  if (g_java_startup_tasks_complete) {
    std::move(callback).Run();
    return;
  }
  CHECK(!GetJavaStartupTasksCompleteCallback());
  GetJavaStartupTasksCompleteCallback() = std::move(callback);
}

void SetActivityTeardownCompleteCallbackForBrowserTests(
    base::OnceClosure callback) {
  if (g_activity_teardown_complete) {
    std::move(callback).Run();
    return;
  }
  CHECK(!GetActivityTeardownCompleteCallback());
  GetActivityTeardownCompleteCallback() = std::move(callback);
}

void RunActivityTeardownCallback() {
  if (g_activity_teardown_started) {
    return;
  }
  g_activity_teardown_started = true;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NativeBrowserTest_runActivityTeardownCallback(env);
}

}  // namespace android
}  // namespace testing

DEFINE_JNI(NativeBrowserTest)
