// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_CHILD_PROCESS_IMPORTANCE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_CHILD_PROCESS_IMPORTANCE_H_

#include "content/common/content_export.h"

namespace content {

// Importance of a child process. For renderer processes, the importance is
// independent from visibility of its WebContents.
// Values are listed in increasing importance.
//
// Each importance results in a service binding with different service binding
// flags to tell the priority of the renderer process to Android system.
//
// PERCEPTIBLE importance leads to a service binding with
// `Context.BIND_NOT_PERCEPTIBLE` which is supported on Android Q+. On older
// Android version, PERCEPTIBLE importance falls back to NORMAL importance and
// the corresponding waived service binding.
//
// Note that the numerical order in ChildProcessImportance should be consistent
// because ChildProcessImportance is compared numerically in
// ChildProcessRanking.java.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class ChildProcessImportance {
  // NORMAL is the default value.
  NORMAL = 0,
  PERCEPTIBLE,
  MODERATE,
  IMPORTANT,
};

// Whether the device supports `ChildProcessImportance.PERCEPTIBLE` or not.
CONTENT_EXPORT bool IsPerceptibleImportanceSupported();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_CHILD_PROCESS_IMPORTANCE_H_
