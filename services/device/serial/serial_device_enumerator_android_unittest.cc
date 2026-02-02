// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_android.h"

#include <fcntl.h>

#include <cstdint>

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/serial/native_unittests_jni_headers/CppTestHelper_jni.h"

namespace device {

class SerialDeviceEnumeratorAndroidTest : public testing::Test {
 public:
  std::unique_ptr<SerialDeviceEnumeratorAndroid> CreateEnumeratorForTesting() {
    auto enumerator = std::make_unique<SerialDeviceEnumeratorAndroid>();
    JNIEnv* env = jni_zero::AttachCurrentThread();
    enumerator->SetSerialManagerForTesting(
        Java_CppTestHelper_createFakeSerialManager(
            env, reinterpret_cast<int64_t>(enumerator.get())));
    return enumerator;
  }

  // Get a safe file descriptor for test purposes.
  int GetSafeFd() { return open("/dev/null", O_RDONLY | O_CLOEXEC); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

TEST_F(SerialDeviceEnumeratorAndroidTest, GetDevices) {
  std::unique_ptr<SerialDeviceEnumeratorAndroid> enumerator =
      CreateEnumeratorForTesting();

  std::vector<mojom::SerialPortInfoPtr> devices = enumerator->GetDevices();

  std::sort(
      devices.begin(), devices.end(),
      [](const auto& lhs, const auto& rhs) { return lhs->path < rhs->path; });
  ASSERT_EQ(devices.size(), 2u);

  EXPECT_FALSE(devices[0]->serial_number.has_value());
  EXPECT_EQ(devices[0]->path, base::FilePath("serial:/ttyACM0"));
  EXPECT_TRUE(devices[0]->has_vendor_id);
  EXPECT_EQ(devices[0]->vendor_id, 0x0694);
  EXPECT_TRUE(devices[0]->has_product_id);
  EXPECT_EQ(devices[0]->product_id, 0x0009);
  EXPECT_FALSE(devices[0]->display_name.has_value());

  EXPECT_FALSE(devices[1]->serial_number.has_value());
  EXPECT_EQ(devices[1]->path, base::FilePath("serial:/ttyS9"));
  EXPECT_FALSE(devices[1]->has_vendor_id);
  EXPECT_FALSE(devices[1]->has_product_id);
  EXPECT_FALSE(devices[1]->display_name.has_value());
}

TEST_F(SerialDeviceEnumeratorAndroidTest, OpenPath_NotFound) {
  std::unique_ptr<SerialDeviceEnumeratorAndroid> enumerator =
      CreateEnumeratorForTesting();
  std::optional<base::ScopedFD> fd_opt;
  std::optional<std::string> error_opt;
  OpenPathCallback callback = base::BindLambdaForTesting(
      [&](base::ScopedFD fd) mutable { fd_opt = std::move(fd); });
  ErrorCallback error_callback = base::BindLambdaForTesting(
      [&](const std::string& error_name, const std::string& message) mutable {
        error_opt = error_name;
      });

  enumerator->OpenPath(base::FilePath("serial:/not_found"), std::move(callback),
                       std::move(error_callback));

  EXPECT_FALSE(fd_opt.has_value());
  EXPECT_TRUE(error_opt.has_value());
}

TEST_F(SerialDeviceEnumeratorAndroidTest, OpenPath_Error) {
  std::unique_ptr<SerialDeviceEnumeratorAndroid> enumerator =
      CreateEnumeratorForTesting();
  std::optional<base::ScopedFD> fd_opt;
  std::optional<std::string> error_opt;
  OpenPathCallback callback = base::BindLambdaForTesting(
      [&](base::ScopedFD fd) mutable { fd_opt = std::move(fd); });
  ErrorCallback error_callback = base::BindLambdaForTesting(
      [&](const std::string& error_name, const std::string& message) mutable {
        error_opt = error_name;
      });
  JNIEnv* env = jni_zero::AttachCurrentThread();

  enumerator->OpenPath(base::FilePath("serial:/ttyS9"), std::move(callback),
                       std::move(error_callback));
  Java_CppTestHelper_invokeErrorCallback(env, "ttyS9", "open_error");

  EXPECT_FALSE(fd_opt.has_value());
  EXPECT_TRUE(error_opt.has_value());
}

TEST_F(SerialDeviceEnumeratorAndroidTest, OpenPath_Success) {
  std::unique_ptr<SerialDeviceEnumeratorAndroid> enumerator =
      CreateEnumeratorForTesting();
  std::optional<base::ScopedFD> fd_opt;
  std::optional<std::string> error_opt;
  OpenPathCallback callback = base::BindLambdaForTesting(
      [&](base::ScopedFD fd) mutable { fd_opt = std::move(fd); });
  ErrorCallback error_callback = base::BindLambdaForTesting(
      [&](const std::string& error_name, const std::string& message) mutable {
        error_opt = error_name;
      });
  JNIEnv* env = jni_zero::AttachCurrentThread();
  int fd = GetSafeFd();

  enumerator->OpenPath(base::FilePath("serial:/ttyS9"), std::move(callback),
                       std::move(error_callback));
  Java_CppTestHelper_invokeOpenPathCallback(env, "ttyS9", fd);

  EXPECT_TRUE(fd_opt.has_value());
  EXPECT_EQ(fd_opt.value(), fd);
  EXPECT_FALSE(error_opt.has_value());
}

}  // namespace device
