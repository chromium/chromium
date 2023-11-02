// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_
#define CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_

#include <jni.h>

#include "base/android/base_jni_onload.h"
#include "content/common/content_export.h"

namespace content {
namespace android {

// Returns true if initialization succeeded.
CONTENT_EXPORT bool OnJNIOnLoadInit();

}  // namespace android
}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_
