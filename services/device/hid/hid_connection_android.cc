// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "services/device/hid/jni_headers/ChromeHidConnection_jni.h"
#include "third_party/jni_zero/default_conversions.h"

DEFINE_JNI(ChromeHidConnection)

namespace device {

using ::base::android::AttachCurrentThread;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaByteArray;

HidConnectionAndroid::HidConnectionAndroid(
    scoped_refptr<HidDeviceInfo> device_info,
    bool allow_protected_reports,
    bool allow_fido_reports,
    base::android::ScopedJavaGlobalRef<jobject> j_connection)
    : HidConnection(std::move(device_info),
                    allow_protected_reports,
                    allow_fido_reports),
      j_connection_(std::move(j_connection)) {
  CHECK(!j_connection_.is_null());
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeHidConnection_setNativeConnection(
      env, j_connection_, reinterpret_cast<intptr_t>(this));
}

HidConnectionAndroid::~HidConnectionAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PlatformClose();
}

void HidConnectionAndroid::PlatformClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeHidConnection_close(env, j_connection_);

  // Fail all pending callbacks
  auto pending_writes = std::move(pending_writes_);
  for (auto& [id, callback] : pending_writes) {
    std::move(callback).Run(false);
  }

  auto pending_feature_reads = std::move(pending_feature_reads_);
  for (auto& [id, callback] : pending_feature_reads) {
    std::move(callback).Run(false, nullptr, 0);
  }
}

void HidConnectionAndroid::PlatformWrite(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(buffer->size(), 1u);

  uint32_t callback_id = next_callback_id_++;
  pending_writes_[callback_id] = std::move(callback);

  uint8_t report_id = buffer->data()[0];
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_data;
  if (buffer->size() > 1) {
    j_data = ToJavaByteArray(env, base::span(buffer->as_vector()).subspan(1u));
  } else {
    j_data = ToJavaByteArray(env, base::span<const uint8_t>());
  }

  Java_ChromeHidConnection_sendOutputReport(env, j_connection_, report_id,
                                            j_data, callback_id);
}

void HidConnectionAndroid::PlatformSendFeatureReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(buffer->size(), 1u);

  uint32_t callback_id = next_callback_id_++;
  pending_writes_[callback_id] = std::move(callback);

  uint8_t report_id = buffer->data()[0];
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_data;
  if (buffer->size() > 1) {
    j_data = ToJavaByteArray(env, base::span(buffer->as_vector()).subspan(1u));
  } else {
    j_data = ToJavaByteArray(env, base::span<const uint8_t>());
  }

  Java_ChromeHidConnection_sendFeatureReport(env, j_connection_, report_id,
                                             j_data, callback_id);
}

void HidConnectionAndroid::PlatformGetFeatureReport(uint8_t report_id,
                                                    ReadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint32_t callback_id = next_callback_id_++;
  pending_feature_reads_[callback_id] = std::move(callback);

  JNIEnv* env = AttachCurrentThread();
  Java_ChromeHidConnection_getFeatureReport(env, j_connection_, report_id,
                                            callback_id);
}

void HidConnectionAndroid::OnWriteComplete(uint32_t callback_id, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto write_it = pending_writes_.find(callback_id);
  if (write_it != pending_writes_.end()) {
    WriteCallback callback = std::move(write_it->second);
    pending_writes_.erase(write_it);
    std::move(callback).Run(success);
  }
}

void HidConnectionAndroid::OnReadFeatureComplete(
    uint32_t callback_id,
    bool success,
    uint32_t report_id,
    const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_feature_reads_.find(callback_id);
  if (it == pending_feature_reads_.end()) {
    return;
  }

  ReadCallback callback = std::move(it->second);
  pending_feature_reads_.erase(it);

  if (!success) {
    std::move(callback).Run(false, nullptr, 0);
    return;
  }

  scoped_refptr<base::RefCountedBytes> buffer;
  size_t size = data.size();

  // In WebHID and Chromium's cross-platform HID implementation
  // (HidConnectionImpl::OnGetFeatureReport), buffer->data()[0] MUST ALWAYS
  // contain the report ID (even if report_id == 0).
  buffer = base::MakeRefCounted<base::RefCountedBytes>(size + 1);
  buffer->as_vector()[0] = report_id;
  if (size > 0) {
    base::span(buffer->as_vector()).subspan(1u).copy_from(data);
  }
  size++;

  std::move(callback).Run(true, std::move(buffer), size);
}

void HidConnectionAndroid::OnInputReport(
    JNIEnv* env,
    uint8_t report_id,
    const base::android::JavaRef<jbyteArray>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // HidConnection::ProcessInputReport expects a RefCountedBytes buffer where
  // byte 0 contains the report ID, followed by payload data.
  size_t data_size = data.GetSize(env);
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(1 + data_size);
  buffer->as_vector()[0] = report_id;
  if (data_size > 0) {
    data.CopyTo(env, base::span(buffer->as_vector()).subspan(1u).data(),
                data_size);
  }
  ProcessInputReport(std::move(buffer), 1 + data_size);
}

}  // namespace device
