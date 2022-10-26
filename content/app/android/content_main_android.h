// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_ANDROID_CONTENT_MAIN_ANDROID_H_
#define CONTENT_APP_ANDROID_CONTENT_MAIN_ANDROID_H_

#include "content/common/content_export.h"

namespace content {

class ContentMainDelegate;

// Allows the test launcher to retrieve the delegate set through
// SetContentMainDelegate().
CONTENT_EXPORT ContentMainDelegate* GetContentMainDelegateForTesting();

}  // namespace content

#endif  // CONTENT_APP_ANDROID_CONTENT_MAIN_ANDROID_H_
