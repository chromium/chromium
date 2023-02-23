// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"

namespace content {

// This class is responsible for communicating with java code to handle
// registering events received on the web with Android.
class AttributionOsLevelManagerAndroid : public AttributionOsLevelManager {
 public:
  AttributionOsLevelManagerAndroid();
  ~AttributionOsLevelManagerAndroid() override;

  AttributionOsLevelManagerAndroid(const AttributionOsLevelManagerAndroid&) =
      delete;
  AttributionOsLevelManagerAndroid& operator=(
      const AttributionOsLevelManagerAndroid&) = delete;

  AttributionOsLevelManagerAndroid(AttributionOsLevelManagerAndroid&&) = delete;
  AttributionOsLevelManagerAndroid& operator=(
      AttributionOsLevelManagerAndroid&&) = delete;

  void RegisterAttributionSource(const GURL& registration_url,
                                 const url::Origin& top_level_origin,
                                 bool is_debug_key_allowed) override;

  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 const std::set<url::Origin>& origins,
                 const std::set<std::string>& domains,
                 BrowsingDataFilterBuilder::Mode mode,
                 bool delete_rate_limit_data,
                 base::OnceClosure done) override;

  attribution_reporting::mojom::OsSupport GetOsSupport() override;

  // This is exposed to JNI and therefore has to be public.
  void OnDataDeletionCompleted(JNIEnv* env, jint request_id);

 private:
  base::flat_map<int, base::OnceClosure> pending_data_deletion_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::android::ScopedJavaGlobalRef<jobject> jobj_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_
