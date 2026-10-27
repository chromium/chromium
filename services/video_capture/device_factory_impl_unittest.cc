// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/capture/video/video_capture_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {
namespace {

class FakeVideoCaptureSystem : public media::VideoCaptureSystem {
 public:
  void GetDeviceInfosAsync(
      media::mojom::VideoCaptureDeviceEnumerationMode enumeration_mode,
      DeviceInfoCallback result_callback) override {
    enumeration_modes_.push_back(enumeration_mode);
    callbacks_.push_back(std::move(result_callback));
  }

  bool HasDeviceInfosWithCapabilities() const override {
    return has_device_infos_with_capabilities_;
  }

  void SetHasDeviceInfosWithCapabilities(bool value) {
    has_device_infos_with_capabilities_ = value;
  }

  media::VideoCaptureErrorOrDevice CreateDevice(const std::string&) override {
    ++create_device_count_;
    return media::VideoCaptureErrorOrDevice(
        media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
  }

  media::VideoCaptureDeviceFactory* GetFactory() override { return nullptr; }

  void CompleteNextEnumeration() {
    ASSERT_FALSE(callbacks_.empty());
    DeviceInfoCallback callback = std::move(callbacks_.front());
    callbacks_.erase(callbacks_.begin());
    std::move(callback).Run({});
  }

  void CompletePendingEnumerations() {
    std::vector<DeviceInfoCallback> callbacks = std::move(callbacks_);
    callbacks_.clear();
    for (DeviceInfoCallback& callback : callbacks) {
      std::move(callback).Run({});
    }
  }

  const std::vector<media::mojom::VideoCaptureDeviceEnumerationMode>&
  enumeration_modes() const {
    return enumeration_modes_;
  }

  int create_device_count() const { return create_device_count_; }

 private:
  std::vector<media::mojom::VideoCaptureDeviceEnumerationMode>
      enumeration_modes_;
  std::vector<DeviceInfoCallback> callbacks_;
  bool has_device_infos_with_capabilities_ = false;
  int create_device_count_ = 0;
};

std::unique_ptr<DeviceFactoryImpl> CreateTestDeviceFactory(
    std::unique_ptr<media::VideoCaptureSystem> capture_system) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<DeviceFactoryImpl>(
      std::move(capture_system), base::DoNothing(),
      base::SequencedTaskRunner::GetCurrentDefault());
#else
  return std::make_unique<DeviceFactoryImpl>(std::move(capture_system));
#endif
}

class DeviceFactoryImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto capture_system = std::make_unique<FakeVideoCaptureSystem>();
    capture_system_ = capture_system.get();
    device_factory_ = CreateTestDeviceFactory(std::move(capture_system));
  }

  void EnumerateDevices(
      media::mojom::VideoCaptureDeviceEnumerationMode enumeration_mode) {
    device_factory_->GetDeviceInfos(enumeration_mode, base::DoNothing());
    capture_system_->SetHasDeviceInfosWithCapabilities(
        enumeration_mode == media::mojom::VideoCaptureDeviceEnumerationMode::
                                kDevicesWithCapabilities);
    capture_system_->CompleteNextEnumeration();
  }

  void RequestDeviceCreation() {
    device_factory_->CreateDevice(
        "device_id",
        base::BindLambdaForTesting([this](DeviceFactory::DeviceInfo info) {
          EXPECT_EQ(
              info.result_code,
              media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound);
          ++create_device_callback_count_;
        }));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<DeviceFactoryImpl> device_factory_;
  raw_ptr<FakeVideoCaptureSystem> capture_system_ = nullptr;
  int create_device_callback_count_ = 0;
};

TEST_F(DeviceFactoryImplTest,
       CapabilitiesEnumerationAllowsDeviceCreationWithoutAnotherEnumeration) {
  EnumerateDevices(media::mojom::VideoCaptureDeviceEnumerationMode::
                       kDevicesWithCapabilities);

  RequestDeviceCreation();
  EXPECT_EQ(create_device_callback_count_, 1);
  EXPECT_EQ(capture_system_->enumeration_modes().size(), 1u);
  EXPECT_EQ(capture_system_->create_device_count(), 1);
}

#if BUILDFLAG(IS_WIN)
TEST_F(DeviceFactoryImplTest,
       DevicesOnlyEnumerationRequiresCapabilitiesBeforeDeviceCreation) {
  EnumerateDevices(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly);

  RequestDeviceCreation();
  EXPECT_EQ(create_device_callback_count_, 0);
  ASSERT_EQ(capture_system_->enumeration_modes().size(), 2u);
  EXPECT_EQ(capture_system_->enumeration_modes().back(),
            media::mojom::VideoCaptureDeviceEnumerationMode::
                kDevicesWithCapabilities);
  EXPECT_EQ(capture_system_->create_device_count(), 0);

  capture_system_->CompleteNextEnumeration();
  EXPECT_EQ(capture_system_->create_device_count(), 1);
  EXPECT_EQ(create_device_callback_count_, 1);
}

TEST_F(DeviceFactoryImplTest,
       DevicesOnlyEnumerationInvalidatesCapabilitiesForDeviceCreation) {
  EnumerateDevices(media::mojom::VideoCaptureDeviceEnumerationMode::
                       kDevicesWithCapabilities);
  EnumerateDevices(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly);

  RequestDeviceCreation();
  EXPECT_EQ(create_device_callback_count_, 0);
  ASSERT_EQ(capture_system_->enumeration_modes().size(), 3u);
  EXPECT_EQ(capture_system_->enumeration_modes().back(),
            media::mojom::VideoCaptureDeviceEnumerationMode::
                kDevicesWithCapabilities);
  EXPECT_EQ(capture_system_->create_device_count(), 0);

  capture_system_->CompleteNextEnumeration();
  EXPECT_EQ(capture_system_->create_device_count(), 1);
  EXPECT_EQ(create_device_callback_count_, 1);
}

TEST_F(DeviceFactoryImplTest,
       DeviceCreationWaitsForCapabilitiesEnumerationToComplete) {
  EnumerateDevices(
      media::mojom::VideoCaptureDeviceEnumerationMode::kDevicesOnly);

  RequestDeviceCreation();
  RequestDeviceCreation();
  EXPECT_EQ(create_device_callback_count_, 0);
  EXPECT_EQ(capture_system_->create_device_count(), 0);

  capture_system_->CompletePendingEnumerations();
  EXPECT_EQ(create_device_callback_count_, 2);
  EXPECT_EQ(capture_system_->create_device_count(), 2);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace video_capture
