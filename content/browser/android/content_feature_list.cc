// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "content/public/android/content_jni_headers/ContentFeatureListImpl_jni.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_features.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace content {
namespace android {

namespace {

// Array of features exposed through the Java ContentFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kAccessibilityPageZoom,
    &features::kAutoDisableAccessibility,
    &features::kAutoDisableAccessibilityV2,
    &features::kBackgroundMediaRendererHasModerateBinding,
    &features::kBindingManagerConnectionLimit,
    &features::kBindingManagerUseNotPerceptibleBinding,
    &features::kComputeAXMode,
    &features::kFedCm,
    &features::kOnDemandAccessibilityEvents,
    &features::kProcessSharingWithStrictSiteInstances,
    &features::kReduceGpuPriorityOnBackground,
    &features::kRequestDesktopSiteAdditions,
    &features::kRequestDesktopSiteExceptions,
    &features::kTouchDragAndContextMenu,
    &features::kWebAuthConditionalUI,
    &features::kWebAuthnTouchToFillCredentialSelection,
    &features::kWebBluetoothNewPermissionsBackend,
    &features::kWebNfc,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED() << "Queried feature cannot be found in ContentFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

static jboolean JNI_ContentFeatureListImpl_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

static jint JNI_ContentFeatureListImpl_GetFieldTrialParamByFeatureAsInt(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jint jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsInt(*feature, param_name,
                                                jdefault_value);
}

static jboolean JNI_ContentFeatureListImpl_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jboolean jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 jdefault_value);
}

}  // namespace android
}  // namespace content
