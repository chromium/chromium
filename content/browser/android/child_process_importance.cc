// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/child_process_importance.h"

#include "base/android/android_info.h"

namespace content {
bool IsPerceptibleImportanceSupported() {
  // This is the same as `SUPPORT_NOT_PERCEPTIBLE_BINDING` in
  // ChildProcessConnection.java.
  return base::android::android_info::sdk_int() >=
         base::android::android_info::SDK_VERSION_Q;
}
}  // namespace content
