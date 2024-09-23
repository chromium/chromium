// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_ANDROID_UTIL_H_
#define MEDIA_BASE_ANDROID_ANDROID_UTIL_H_

#include <memory>

#include "base/android/scoped_java_ref.h"

namespace media {

// TODO(crbug.com/40540372): Remove the type. Use ScopedJavaGlobalRef directly.
using JavaObjectPtr =
    std::unique_ptr<base::android::ScopedJavaGlobalRef<jobject>>;

// A helper method to create a JavaObjectPtr.
JavaObjectPtr CreateJavaObjectPtr(jobject object);

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ANDROID_UTIL_H_
