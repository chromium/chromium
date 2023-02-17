// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace content {

// This class is responsible for communicating with java code to handle
// registering events received on the web with Android.
class AttributionOsLevelManagerAndroid {
 public:
  AttributionOsLevelManagerAndroid();
  ~AttributionOsLevelManagerAndroid();

  AttributionOsLevelManagerAndroid(const AttributionOsLevelManagerAndroid&) =
      delete;
  AttributionOsLevelManagerAndroid& operator=(
      const AttributionOsLevelManagerAndroid&) = delete;

  AttributionOsLevelManagerAndroid(AttributionOsLevelManagerAndroid&&) = delete;
  AttributionOsLevelManagerAndroid& operator=(
      AttributionOsLevelManagerAndroid&&) = delete;

  void RegisterAttributionSource(const GURL& registration_url,
                                 const url::Origin& top_level_origin,
                                 bool is_debug_key_allowed);

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OS_LEVEL_MANAGER_ANDROID_H_
