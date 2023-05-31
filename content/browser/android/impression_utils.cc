// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/impression_utils.h"

#include "base/android/unguessable_token_android.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/android/content_jni_headers/ImpressionUtils_jni.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> CreateJavaImpression(
    JNIEnv* env,
    base::UnguessableToken attribution_src_token,
    base::UnguessableToken initiator_frame_token,
    int initiator_process_id,
    const network::AttributionReportingRuntimeFeatures& features) {
  return Java_ImpressionUtils_create(
      env,
      base::android::UnguessableTokenAndroid::Create(env,
                                                     attribution_src_token),
      base::android::UnguessableTokenAndroid::Create(env,
                                                     initiator_frame_token),
      initiator_process_id, features.ToEnumBitmask());
}

network::AttributionReportingRuntimeFeatures
GetAttributionRuntimeFeaturesFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return network::AttributionReportingRuntimeFeatures();
  }
  return network::AttributionReportingRuntimeFeatures::FromEnumBitmask(
      base::checked_cast<uint64_t>(
          Java_ImpressionUtils_getAttributionRuntimeFeatures(env, j_object)));
}

absl::optional<blink::LocalFrameToken> GetInitiatorFrameTokenFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return absl::nullopt;
  }
  auto optional_token =
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
          env, Java_ImpressionUtils_getInitiatorFrameToken(env, j_object));
  if (optional_token) {
    return blink::LocalFrameToken(optional_token.value());
  }
  return absl::nullopt;
}

int GetInitiatorProcessIDFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return false;
  }
  return Java_ImpressionUtils_getInitiatorProcessID(env, j_object);
}

absl::optional<blink::AttributionSrcToken>
GetAttributionSrcTokenFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return absl::nullopt;
  }
  auto optional_token =
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
          env, Java_ImpressionUtils_getAttributionSrcToken(env, j_object));
  if (optional_token) {
    return blink::AttributionSrcToken(optional_token.value());
  }
  return absl::nullopt;
}

}  // namespace content
