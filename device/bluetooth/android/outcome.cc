// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/android/outcome.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/compiler_specific.h"
#include "device/bluetooth/jni_headers/Outcome_jni.h"

namespace device {

Outcome::Outcome(base::android::ScopedJavaLocalRef<jobject> j_outcome)
    : j_outcome_(j_outcome) {}

Outcome::~Outcome() = default;

bool Outcome::IsSuccessful() const {
  return Java_Outcome_isSuccessful(base::android::AttachCurrentThread(),
                                   j_outcome_);
}

Outcome::operator bool() const {
  return IsSuccessful();
}

base::android::ScopedJavaLocalRef<jobject> Outcome::GetResult() const {
  return Java_Outcome_getResult(base::android::AttachCurrentThread(),
                                j_outcome_);
}

int Outcome::GetIntResult() const {
  return Java_Outcome_getIntResult(base::android::AttachCurrentThread(),
                                   j_outcome_);
}

std::string Outcome::GetExceptionMessage() const {
  return Java_Outcome_getExceptionMessage(base::android::AttachCurrentThread(),
                                          j_outcome_);
}

}  // namespace device
