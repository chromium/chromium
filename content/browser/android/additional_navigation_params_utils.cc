// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/additional_navigation_params_utils.h"

#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/AdditionalNavigationParamsUtils_jni.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> CreateJavaAdditionalNavigationParams(
    JNIEnv* env,
    base::UnguessableToken initiator_frame_token,
    int initiator_process_id,
    std::optional<base::UnguessableToken> attribution_src_token) {
  return Java_AdditionalNavigationParamsUtils_create(
      env, initiator_frame_token, initiator_process_id, attribution_src_token);
}

std::optional<blink::LocalFrameToken>
GetInitiatorFrameTokenFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return std::nullopt;
  }
  std::optional<base::UnguessableToken> optional_token =
      Java_AdditionalNavigationParamsUtils_getInitiatorFrameToken(env,
                                                                  j_object);
  if (optional_token) {
    return blink::LocalFrameToken(optional_token.value());
  }
  return std::nullopt;
}

int GetInitiatorProcessIdFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return false;
  }
  return Java_AdditionalNavigationParamsUtils_getInitiatorProcessId(env,
                                                                    j_object);
}

std::optional<blink::AttributionSrcToken>
GetAttributionSrcTokenFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return std::nullopt;
  }
  std::optional<base::UnguessableToken> optional_token =
      Java_AdditionalNavigationParamsUtils_getAttributionSrcToken(env,
                                                                  j_object);
  if (optional_token) {
    return blink::AttributionSrcToken(optional_token.value());
  }
  return std::nullopt;
}

}  // namespace content
