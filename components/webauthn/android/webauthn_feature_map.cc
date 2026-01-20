// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/webauthn/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webauthn/android/jni_headers/WebauthnFeatureMap_jni.h"

namespace webauthn::features {

namespace {

// Array of features exposed through the Java WebauthnFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &kWebAuthnAndroidPasskeyCacheMigration,
    &kWebAuthnAndroidCredManForDev,
    &kWebAuthnAndroidCredManRequestExtraBundle,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_WebauthnFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace webauthn::features

DEFINE_JNI(WebauthnFeatureMap)
