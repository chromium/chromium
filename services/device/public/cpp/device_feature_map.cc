// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "device/fido/features.h"
#include "services/device/public/cpp/device_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/public/java/device_feature_list_jni/DeviceFeatureMap_jni.h"

namespace features {

namespace {

// Array of features exposed through the Java DeviceFeatureMap API. Entries in
// this array may either refer to features defined in
// services/device/public/cpp/device_features.h or in other locations in the
// code base.
const base::Feature* const kFeaturesExposedToJava[] = {
    &device::kWebAuthnAndroidCredMan,
    &kGenericSensorExtraClasses,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_DeviceFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace features
