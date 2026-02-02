// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_ANDROID_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_ANDROID_H_

#include <jni.h>

#include <cstdint>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "services/device/serial/serial_device_enumerator.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace device {

// An OpenPathCallback callback is run when an OpenPath request is completed.
using OpenPathCallback = base::OnceCallback<void(base::ScopedFD)>;

// An ErrorCallback callback is run when OpenPath request fails.
using ErrorCallback = base::OnceCallback<void(const std::string& error_name,
                                              const std::string& message)>;

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorAndroid : public SerialDeviceEnumerator {
 public:
  class Callbacks;

  SerialDeviceEnumeratorAndroid();

  SerialDeviceEnumeratorAndroid(const SerialDeviceEnumeratorAndroid&) = delete;
  SerialDeviceEnumeratorAndroid& operator=(
      const SerialDeviceEnumeratorAndroid&) = delete;

  ~SerialDeviceEnumeratorAndroid() override;

  // Must be called once immediately after the constructor.
  void Initialize();

  scoped_refptr<SerialIoHandler> CreateIoHandler(
      const base::FilePath& path,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) override;

  // OpenPath requests opening the device node identified by |path| and
  // returning the resulting file descriptor.
  // One of |callback| or |error_callback| is called.
  void OpenPath(const base::FilePath& path,
                OpenPathCallback callback,
                ErrorCallback error_callback);

  // Methods called by Java.
  void OpenPathCallbackViaJni(JNIEnv* env,
                              const std::string& port_name,
                              int32_t fd);
  void ErrorCallbackViaJni(JNIEnv* env,
                           const std::string& port_name,
                           const std::string& error_name,
                           const std::string& message);
  void AddPortViaJni(JNIEnv* env,
                     const std::string& name,
                     int32_t vendor_id,
                     int32_t product_id,
                     bool initial_enumeration);
  void RemovePortViaJni(JNIEnv* env, const std::string& name);

  // For testing
  void SetSerialManagerForTesting(
      jni_zero::ScopedJavaLocalRef<jobject> j_serial_manager);

 private:
  void AddPortName(const std::string& name, int vendor_id, int product_id);
  void RemovePortName(const std::string& name);

  // Maps device names to tokens used in SerialDeviceEnumerator.
  absl::flat_hash_map<std::string, base::UnguessableToken> tokens_;

  // Maps device names to a pair of callbacks during |OpenPath|. */
  absl::flat_hash_map<std::string, std::unique_ptr<Callbacks>> callbacks_;

  // Java object org.chromium.device.serial.ChromeSerialManager.
  base::android::ScopedJavaGlobalRef<jobject> j_serial_manager_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SerialDeviceEnumeratorAndroid> weak_factory_{this};
};

// Contains a pair of callbacks for |OpenPath| operation.
class SerialDeviceEnumeratorAndroid::Callbacks {
 public:
  Callbacks(OpenPathCallback success_callback, ErrorCallback error_callback);

  Callbacks(const Callbacks&) = delete;
  Callbacks& operator=(const Callbacks&) = delete;

  ~Callbacks();

  void Success(base::ScopedFD fd);

  void Error(const std::string& error_name, const std::string& message);

 private:
  OpenPathCallback success_callback_;
  ErrorCallback error_callback_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_ANDROID_H_
