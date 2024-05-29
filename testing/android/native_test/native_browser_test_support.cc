// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class sets up the environment for running the native tests inside an
// android application. It outputs (to a fifo) markers identifying the
// START/PASSED/CRASH of the test suite, FAILURE/SUCCESS of individual tests,
// etc.
// These markers are read by the test runner script to generate test results.
// It installs signal handlers to detect crashes.

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "testing/android/native_test/native_browser_test_jni/NativeBrowserTest_jni.h"

namespace testing {
namespace android {

namespace {

// Java calls to set this true when async startup tasks are done for browser
// tests.
bool g_java_startup_tasks_complete = false;

}  // namespace

void JNI_NativeBrowserTest_JavaStartupTasksCompleteForBrowserTests(
    JNIEnv* env) {
  g_java_startup_tasks_complete = true;
}

bool JavaAsyncStartupTasksCompleteForBrowserTests() {
  return g_java_startup_tasks_complete;
}

}  // namespace android
}  // namespace testing
