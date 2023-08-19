// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/additional_navigation_params_utils.h"

#include "base/android/unguessable_token_android.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/android/content_jni_headers/AdditionalNavigationParamsUtils_jni.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> CreateJavaAdditionalNavigationParams(
    JNIEnv* env,
    base::UnguessableToken initiator_frame_token,
    int initiator_process_id,
    absl::optional<base::UnguessableToken> attribution_src_token,
    absl::optional<network::AttributionReportingRuntimeFeatures>
        runtime_features) {
  return Java_AdditionalNavigationParamsUtils_create(
      env,
      base::android::UnguessableTokenAndroid::Create(env,
                                                     initiator_frame_token),
      initiator_process_id,
      attribution_src_token ? base::android::UnguessableTokenAndroid::Create(
                                  env, attribution_src_token.value())
                            : nullptr,
      runtime_features ? runtime_features->ToEnumBitmask() : 0);
}

absl::optional<blink::LocalFrameToken>
GetInitiatorFrameTokenFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return absl::nullopt;
  }
  auto optional_token =
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
          env, Java_AdditionalNavigationParamsUtils_getInitiatorFrameToken(
                   env, j_object));
  if (optional_token) {
    return blink::LocalFrameToken(optional_token.value());
  }
  return absl::nullopt;
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

absl::optional<blink::AttributionSrcToken>
GetAttributionSrcTokenFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return absl::nullopt;
  }
  auto java_token = Java_AdditionalNavigationParamsUtils_getAttributionSrcToken(
      env, j_object);
  if (!java_token) {
    return absl::nullopt;
  }
  auto optional_token =
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
          env, java_token);
  if (optional_token) {
    return blink::AttributionSrcToken(optional_token.value());
  }
  return absl::nullopt;
}

network::AttributionReportingRuntimeFeatures
GetAttributionRuntimeFeaturesFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  if (!j_object) {
    return network::AttributionReportingRuntimeFeatures();
  }
  return network::AttributionReportingRuntimeFeatures::FromEnumBitmask(
      base::checked_cast<uint64_t>(
          Java_AdditionalNavigationParamsUtils_getAttributionRuntimeFeatures(
              env, j_object)));
}

}  // namespace content
