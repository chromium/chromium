// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_ANDROID_NATIVE_TEST_NATIVE_BROWSER_TEST_SUPPORT_H_
#define TESTING_ANDROID_NATIVE_TEST_NATIVE_BROWSER_TEST_SUPPORT_H_

#include <jni.h>

#include "base/functional/callback_forward.h"

namespace testing {
namespace android {

// Android browser tests must wait for Java async initialization tasks to run
// before running the test. Runs `callback` in the browser process once Java
// async initialization tasks are complete (or immediately if they are already
// complete).
void SetJavaStartupTasksCompleteCallbackForBrowserTests(
    base::OnceClosure callback);

// Asks the Android test harness to tear down the Activity the tests are
// running in. Will cause the callback registered via
// SetActivityTeardownCompleteCallbackForBrowserTests to eventually run.
void RunActivityTeardownCallback();

// Runs `callback` once Activity teardown is complete, as triggered by
// RunActivityTeardownCallback (or immediately if it is already complete).
void SetActivityTeardownCompleteCallbackForBrowserTests(
    base::OnceClosure callback);

}  // namespace android
}  // namespace testing

#endif  // TESTING_ANDROID_NATIVE_TEST_NATIVE_BROWSER_TEST_SUPPORT_H_
