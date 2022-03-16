// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_reporter_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/attribution_reporting/attribution_host_utils.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/renderer_host/navigation_controller_android.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/url_utils.h"
#include "content/public/android/content_jni_headers/AttributionReporterImpl_jni.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "url/origin.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace content {

namespace attribution_reporter_android {

void ReportAppImpression(AttributionManager& attribution_manager,
                         const std::string& source_package_name,
                         const std::string& source_event_id,
                         const std::string& destination,
                         const std::string& report_to,
                         int64_t expiry,
                         base::Time impression_time) {
  absl::optional<blink::Impression> impression =
      attribution_host_utils::ParseImpressionFromApp(
          source_event_id, destination, report_to, expiry);
  if (!impression)
    return;

  url::Origin impression_origin =
      OriginFromAndroidPackageName(source_package_name);

  attribution_host_utils::VerifyAndStoreImpression(
      AttributionSourceType::kEvent, impression_origin, *impression,
      attribution_manager, impression_time);
}

}  // namespace attribution_reporter_android

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
      attribution_host_utils::ParseImpressionFromApp(
          ConvertJavaStringToUTF8(env, j_source_event_id),
          ConvertJavaStringToUTF8(env, j_destination),
          j_report_to ? ConvertJavaStringToUTF8(env, j_report_to) : "", expiry);
  if (!impression)
    return;

  url::Origin impression_origin = OriginFromAndroidPackageName(
      ConvertJavaStringToUTF8(env, j_source_package_name));
  AttributionHost::FromWebContents(web_contents)
      ->ReportAttributionForCurrentNavigation(impression_origin, *impression);
}

void JNI_AttributionReporterImpl_ReportAppImpression(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_browser_context,
    const JavaParamRef<jstring>& j_source_package_name,
    const JavaParamRef<jstring>& j_source_event_id,
    const JavaParamRef<jstring>& j_destination,
    const JavaParamRef<jstring>& j_report_to,
    jlong expiry,
    jlong event_time) {
  BrowserContext* context = BrowserContextFromJavaHandle(j_browser_context);
  DCHECK(context);

  AttributionManager* attribution_manager =
      static_cast<StoragePartitionImpl*>(context->GetDefaultStoragePartition())
          ->GetAttributionManager();
  if (!attribution_manager)
    return;

  base::Time impression_time = event_time == 0
                                   ? base::Time::Now()
                                   : base::Time::FromJavaTime(event_time);

  attribution_reporter_android::ReportAppImpression(
      *attribution_manager, ConvertJavaStringToUTF8(env, j_source_package_name),
      ConvertJavaStringToUTF8(env, j_source_event_id),
      ConvertJavaStringToUTF8(env, j_destination),
      j_report_to ? ConvertJavaStringToUTF8(env, j_report_to) : "", expiry,
      impression_time);
}

}  // namespace content
