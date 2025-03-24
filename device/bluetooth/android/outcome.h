// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ANDROID_OUTCOME_H_
#define DEVICE_BLUETOOTH_ANDROID_OUTCOME_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// Bindings into org.chromium.device.bluetooth.Outcome.
class Outcome final {
 public:
  explicit Outcome(base::android::ScopedJavaLocalRef<jobject> j_outcome);

  Outcome(const Outcome&) = delete;
  Outcome& operator=(const Outcome&) = delete;

  ~Outcome();

  // Returns if this outcome is a successful outcome.
  bool IsSuccessful() const;

  // Returns if this outcome is a successful outcome.
  explicit operator bool() const;

  // Gets the result if this outcome is a successful one. Crashes if the outcome
  // is a failed one.
  base::android::ScopedJavaLocalRef<jobject> GetResult() const;

  // Gets the result as an int. Crashes if the outcome is a failed one.
  int GetIntResult() const;

  // Gets the exception message if the outcome is a failed one. Crashes if the
  // outcome is a successful one.
  std::string GetExceptionMessage() const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_outcome_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_ANDROID_OUTCOME_H_
