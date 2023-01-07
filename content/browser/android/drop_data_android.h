// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DROP_DATA_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_DROP_DATA_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"

namespace content {

// Generate a java equivalent DropData object from |drop_data|. Note that the
// timeline of these object are not equivalent.
base::android::ScopedJavaLocalRef<jobject> ToJavaDropData(
    const DropData& drop_data);

}  // namespace content
#endif  // CONTENT_BROWSER_ANDROID_DROP_DATA_ANDROID_H_
