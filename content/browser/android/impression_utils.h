// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IMPRESSION_UTILS_H_
#define CONTENT_BROWSER_ANDROID_IMPRESSION_UTILS_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/unguessable_token.h"
#include "content/public/browser/android/impression_android.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

network::AttributionReportingRuntimeFeatures
GetAttributionRuntimeFeaturesFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object);

absl::optional<blink::LocalFrameToken> GetInitiatorFrameTokenFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object);

int GetInitiatorProcessIDFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object);

absl::optional<blink::AttributionSrcToken>
GetAttributionSrcTokenFromJavaImpression(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IMPRESSION_UTILS_H_
