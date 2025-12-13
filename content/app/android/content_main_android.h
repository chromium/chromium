// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_ANDROID_CONTENT_MAIN_ANDROID_H_
#define CONTENT_APP_ANDROID_CONTENT_MAIN_ANDROID_H_

#include <cstdint>

#include "content/common/content_export.h"

namespace content {

class ContentMainDelegate;

// Allows the test launcher to retrieve the delegate set through
// SetContentMainDelegate().
CONTENT_EXPORT ContentMainDelegate* GetContentMainDelegateForTesting();

CONTENT_EXPORT int StartContentMain(bool start_minimal_browser);

CONTENT_EXPORT void InitChildProcessCommon(int32_t cpu_count,
                                           int64_t cpu_features);
}  // namespace content

#endif  // CONTENT_APP_ANDROID_CONTENT_MAIN_ANDROID_H_
