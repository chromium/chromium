// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_CHILD_PROCESS_IMPORTANCE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_CHILD_PROCESS_IMPORTANCE_H_

namespace content {

// Importance of a child process. For renderer processes, the importance is
// independent from visibility of its WebContents.
// Values are listed in increasing importance.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class ChildProcessImportance {
  // NORMAL is the default value.
  NORMAL = 0,
  MODERATE,
  IMPORTANT,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_CHILD_PROCESS_IMPORTANCE_H_
