// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/strcat.h"
#include "content/browser/conversions/conversion_host.h"
#include "content/browser/renderer_host/navigation_controller_android.h"
#include "content/public/android/content_jni_headers/AttributionReporterImpl_jni.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/navigation/impression.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace content {

void JNI_AttributionReporterImpl_ReportAttributionForCurrentNavigation(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
    const JavaParamRef<jstring>& j_source_package_name,
    const JavaParamRef<jstring>& j_source_event_id,
    const JavaParamRef<jstring>& j_destination,
    const JavaParamRef<jstring>& j_report_to,
    jlong expiry) {
  WebContents* web_contents = WebContents::FromJavaWebContents(j_web_contents);
  absl::optional<blink::Impression> impression =
      ConversionHost::ParseImpressionFromApp(
          ConvertJavaStringToUTF8(env, j_source_event_id),
          ConvertJavaStringToUTF8(env, j_destination),
          j_report_to ? ConvertJavaStringToUTF8(env, j_report_to) : "", expiry);
  if (!impression)
    return;

  url::Origin impression_origin =
      NavigationControllerAndroid::OriginFromPackageName(
          ConvertJavaStringToUTF8(env, j_source_package_name));
  ConversionHost::FromWebContents(web_contents)
      ->ReportAttributionForCurrentNavigation(impression_origin, *impression);
}

}  // namespace content
