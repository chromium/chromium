// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_IMPRESSION_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_IMPRESSION_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobject> CreateJavaImpression(
    JNIEnv* env,
    base::UnguessableToken attribution_src_token,
    base::UnguessableToken initiator_frame_token,
    int initiator_process_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_IMPRESSION_ANDROID_H_
