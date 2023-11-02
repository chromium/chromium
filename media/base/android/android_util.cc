// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/android_util.h"

#include <stddef.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"

namespace media {

JavaObjectPtr CreateJavaObjectPtr(jobject object) {
  JavaObjectPtr j_object_ptr(new base::android::ScopedJavaGlobalRef<jobject>());
  j_object_ptr->Reset(base::android::AttachCurrentThread(), object);
  return j_object_ptr;
}

}  // namespace media
