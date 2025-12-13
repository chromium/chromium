// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_id.h"

#include <ostream>
#include <string>
#include <string_view>

#include "base/check.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "google_apis/gaia/android/jni_headers/GaiaId_jni.h"
#endif

GaiaId::GaiaId(std::string value) : id_(std::move(value)) {}

bool GaiaId::empty() const {
  return id_.empty();
}

const std::string& GaiaId::ToString() const {
  return id_;
}

std::ostream& operator<<(std::ostream& out, const GaiaId& id) {
  return out << id.ToString();
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaGaiaId(
    JNIEnv* env,
    const GaiaId& gaia_id) {
  CHECK(!gaia_id.empty());
  return Java_GaiaId_Constructor(env, gaia_id.ToString());
}

GaiaId ConvertFromJavaGaiaId(JNIEnv* env,
                             const base::android::JavaRef<jobject>& j_gaia_id) {
  CHECK(j_gaia_id);
  return GaiaId(Java_GaiaId_toString(env, j_gaia_id));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(GaiaId)
#endif
