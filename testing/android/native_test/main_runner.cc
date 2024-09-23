// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/android/binder.h"
#include "base/android/binder_box.h"
#include "base/android/jni_array.h"
#include "base/check.h"
#include "testing/android/native_test/native_test_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "testing/android/native_test/native_main_runner_jni/MainRunner_jni.h"

extern int main(int argc, char** argv);

namespace testing {
namespace android {

static jint JNI_MainRunner_RunMain(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& command_line,
    const base::android::JavaParamRef<jobject>& binder_box) {
  // Guards against process being reused.
  // In most cases, running main again will cause problems (static variables,
  // singletons, lazy instances won't be in the same state as a clean run).
  static bool alreadyRun = false;
  CHECK(!alreadyRun);
  alreadyRun = true;

  if (auto binders = base::android::UnpackBinderBox(env, binder_box);
      binders.has_value()) {
    base::android::SetBindersFromParent(std::move(*binders));
  }

  std::vector<std::string> cpp_command_line;
  base::android::AppendJavaStringArrayToStringVector(env, command_line,
                                                     &cpp_command_line);

  std::vector<char*> argv;
  int argc = ArgsToArgv(cpp_command_line, &argv);
  return main(argc, &argv[0]);
}

}  // namespace android
}  // namespace testing
