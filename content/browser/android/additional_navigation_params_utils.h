// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_UTILS_H_
#define CONTENT_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_UTILS_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/unguessable_token.h"
#include "content/public/browser/android/additional_navigation_params_android.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

std::optional<blink::LocalFrameToken>
GetInitiatorFrameTokenFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object);

int GetInitiatorProcessIdFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object);

std::optional<blink::AttributionSrcToken>
GetAttributionSrcTokenFromJavaAdditionalNavigationParams(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_UTILS_H_
