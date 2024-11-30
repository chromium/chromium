// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_scanner_callback.h"

namespace device {

void BluetoothScannerCallback::OnScanFailed(JNIEnv* env, jint error_code) {
  last_error_code_ = error_code;
}

void BluetoothScannerCallback::OnScanFinished(JNIEnv* env) {
  ++scan_finish_count_;
}

int BluetoothScannerCallback::GetScanFinishCount() {
  return scan_finish_count_;
}

int BluetoothScannerCallback::GetLastErrorCode() {
  return last_error_code_;
}

}  // namespace device
