// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "device/fido/features.h"
#include "services/device/device_service_jni_headers/DeviceFeatureList_jni.h"
#include "services/device/public/cpp/device_features.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace features {

namespace {

// Array of features exposed through the Java ContentFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &device::kWebAuthnAndroidCredMan,
    &device::kWebAuthnHybridLinkWithoutNotifications,
    &kGenericSensorExtraClasses,
    &device::kWebAuthnHybridLinkWithoutNotifications,
};

// TODO(crbug.com/1060097): Removethis once a generalized FeatureList exists.
const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED() << "Queried feature cannot be found in DeviceFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

static jboolean JNI_DeviceFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace features
