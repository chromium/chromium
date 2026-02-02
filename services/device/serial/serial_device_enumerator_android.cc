// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_android.h"

#include <fcntl.h>

#include <cstdint>

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/device_event_log/device_event_log.h"
#include "device/base/features.h"
#include "services/device/serial/serial_io_handler_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/serial/jni_headers/ChromeSerialManager_jni.h"

namespace device {
using ::jni_zero::AttachCurrentThread;

SerialDeviceEnumeratorAndroid::SerialDeviceEnumeratorAndroid()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

void SerialDeviceEnumeratorAndroid::Initialize() {
  if (!base::FeatureList::IsEnabled(features::kWebSerialWiredDevicesAndroid)) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  j_serial_manager_.Reset(
      Java_ChromeSerialManager_create(env, reinterpret_cast<int64_t>(this)));
  if (j_serial_manager_.is_null()) {
    SERIAL_LOG(ERROR) << "Could not find Android Serial Service";
    return;
  }
}

SerialDeviceEnumeratorAndroid::~SerialDeviceEnumeratorAndroid() {
  if (j_serial_manager_.is_null()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_ChromeSerialManager_close(env, j_serial_manager_);
}

scoped_refptr<SerialIoHandler> SerialDeviceEnumeratorAndroid::CreateIoHandler(
    const base::FilePath& path,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return new SerialIoHandlerAndroid(path, std::move(ui_task_runner),
                                    weak_factory_.GetWeakPtr());
}

void SerialDeviceEnumeratorAndroid::OpenPath(const base::FilePath& path,
                                             OpenPathCallback callback,
                                             ErrorCallback error_callback) {
  if (j_serial_manager_.is_null()) {
    std::move(error_callback)
        .Run("Android Serial Service is not available", path.value());
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  const base::FilePath& file_path = path.BaseName();
  const std::string& name = file_path.value();
  if (callbacks_.find(name) != callbacks_.end()) {
    std::move(error_callback).Run("Already opening port", path.value());
    return;
  }
  callbacks_.emplace(name, std::make_unique<Callbacks>(
                               std::move(callback), std::move(error_callback)));
  const std::string& error =
      Java_ChromeSerialManager_openPort(env, j_serial_manager_, name);
  if (!error.empty()) {
    auto node = callbacks_.extract(name);
    if (node.empty()) {
      return;
    }
    std::move(node.mapped())->Error("Error opening port", error);
  }
}

void SerialDeviceEnumeratorAndroid::OpenPathCallbackViaJni(
    JNIEnv* env,
    const std::string& port_name,
    int32_t fd) {
  auto node = callbacks_.extract(port_name);
  if (node.empty()) {
    return;
  }
  std::move(node.mapped())->Success(base::ScopedFD(fd));
}

void SerialDeviceEnumeratorAndroid::ErrorCallbackViaJni(
    JNIEnv* env,
    const std::string& port_name,
    const std::string& error_name,
    const std::string& message) {
  auto node = callbacks_.extract(port_name);
  if (node.empty()) {
    return;
  }
  std::move(node.mapped())->Error(error_name, message);
}

void SerialDeviceEnumeratorAndroid::AddPortViaJni(JNIEnv* env,
                                                  const std::string& name,
                                                  int32_t vendor_id,
                                                  int32_t product_id,
                                                  bool initial_enumeration) {
  if (initial_enumeration) {
    // The class constructor invokes create() via JNI, which invokes this method
    // from Java, in this case we are already on the valid sequence.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    AddPortName(name, vendor_id, product_id);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SerialDeviceEnumeratorAndroid::AddPortName,
                       base::Unretained(this), name, vendor_id, product_id));
  }
}

void SerialDeviceEnumeratorAndroid::RemovePortViaJni(JNIEnv* env,
                                                     const std::string& name) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SerialDeviceEnumeratorAndroid::RemovePortName,
                                base::Unretained(this), name));
}

void SerialDeviceEnumeratorAndroid::AddPortName(const std::string& name,
                                                int vendor_id,
                                                int product_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::SerialPortInfoPtr port = mojom::SerialPortInfo::New();
  port->path = base::FilePath("serial://").Append(name);
  port->vendor_id = vendor_id;
  port->has_vendor_id = (vendor_id >= 0);
  port->product_id = product_id;
  port->has_product_id = (product_id >= 0);
  base::UnguessableToken token = base::UnguessableToken::Create();
  tokens_.try_emplace(name, token);
  port->token = token;
  AddPort(std::move(port));
}

void SerialDeviceEnumeratorAndroid::RemovePortName(const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto node = tokens_.extract(name);
  if (node.empty()) {
    return;
  }
  RemovePort(node.mapped());
}

void SerialDeviceEnumeratorAndroid::SetSerialManagerForTesting(
    jni_zero::ScopedJavaLocalRef<jobject> j_serial_manager) {
  j_serial_manager_.Reset(std::move(j_serial_manager));
}

SerialDeviceEnumeratorAndroid::Callbacks::Callbacks(
    OpenPathCallback success_callback,
    ErrorCallback error_callback)
    : success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

SerialDeviceEnumeratorAndroid::Callbacks::~Callbacks() = default;

void SerialDeviceEnumeratorAndroid::Callbacks::Success(base::ScopedFD fd) {
  std::move(success_callback_).Run(std::move(fd));
}

void SerialDeviceEnumeratorAndroid::Callbacks::Error(
    const std::string& error_name,
    const std::string& message) {
  std::move(error_callback_).Run(error_name, message);
}

}  // namespace device

DEFINE_JNI(ChromeSerialManager)
