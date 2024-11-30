// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_SCANNER_CALLBACK_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_SCANNER_CALLBACK_H_

#include <jni.h>

namespace device {

// Native implementation of
// ChromeBluetoothLeScannerTestUtil.BluetoothScannerCallback. Used only in
// Android unit tests.
class BluetoothScannerCallback {
 public:
  // Called when a scan failed.
  void OnScanFailed(JNIEnv* env, jint error_code);

  // Called when a scan finished.
  void OnScanFinished(JNIEnv* env);

  // Get count of |OnScanFinished| calls.
  int GetScanFinishCount();

  // Get the error code in the last |OnScanFailed| call.
  int GetLastErrorCode();

 private:
  int scan_finish_count_{0};
  int last_error_code_{0};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_SCANNER_CALLBACK_H_
