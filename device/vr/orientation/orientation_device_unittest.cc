// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "device/vr/orientation/orientation_device.h"
#include "device/vr/orientation/orientation_session.h"
#include "device/vr/test/fake_orientation_provider.h"
#include "device/vr/test/fake_sensor_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/quaternion.h"

namespace device {

namespace {

class FakeScreen : public display::Screen {
 public:
  FakeScreen() { display::Screen::SetScreenInstance(this); }
  ~FakeScreen() override { display::Screen::SetScreenInstance(nullptr); }
  display::Display GetPrimaryDisplay() const override { return display; }

  // Unused functions
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  bool IsWindowUnderCursor(gfx::NativeWindow window) override { return false; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return nullptr;
  }
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override {
    return nullptr;
  }
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override {
    return display;
  }
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    return display;
  }
  int GetNumDisplays() const override { return 0; }
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    return display;
  }
  void AddObserver(display::DisplayObserver* observer) override {}
  void RemoveObserver(display::DisplayObserver* observer) override {}
  const std::vector<display::Display>& GetAllDisplays() const override {
    return displays;
  }

  display::Display display;
  const std::vector<display::Display> displays;
};

}  // namespace

class VROrientationDeviceTest : public testing::Test {
 public:
  VROrientationDeviceTest(const VROrientationDeviceTest&) = delete;
  VROrientationDeviceTest& operator=(const VROrientationDeviceTest&) = delete;

  void onDisplaySynced() {}

 protected:
  VROrientationDeviceTest() = default;
  ~VROrientationDeviceTest() override = default;
  void SetUp() override {
    fake_sensor_provider_ = std::make_unique<FakeXRSensorProvider>(
        sensor_provider_.BindNewPipeAndPassReceiver());

    fake_sensor_ = std::make_unique<FakeOrientationSensor>(
        sensor_.InitWithNewPipeAndPassReceiver());

    mapped_region_ = base::ReadOnlySharedMemoryRegion::Create(
        sizeof(SensorReadingSharedBuffer) *
        (static_cast<uint64_t>(mojom::SensorType::kMaxValue) + 1));
    ASSERT_TRUE(mapped_region_.IsValid());

    fake_screen_ = std::make_unique<FakeScreen>();

    task_environment_.RunUntilIdle();
  }

  uint64_t GetBufferOffset() {
    return GetSensorReadingSharedBufferOffset(kOrientationSensorType);
  }

  void InitializeDevice(mojom::SensorInitParamsPtr params) {
    base::RunLoop loop;

    device_ = std::make_unique<VROrientationDevice>(sensor_provider_.get(),
                                                    loop.QuitClosure());

    // Complete the creation of device_ by letting the GetSensor function go
    // through.
    task_environment_.RunUntilIdle();

    fake_sensor_provider_->CallCallback(std::move(params));
    task_environment_.RunUntilIdle();

    // Ensure that the callback is called.
    loop.Run();
  }

  void DeviceReadPose(gfx::Quaternion input_q,
                      base::OnceCallback<void(mojom::VRPosePtr)> callback) {
    // If the device isn't available we can't read a quaternion from it
    ASSERT_TRUE(device_->IsAvailable());

    WriteToBuffer(input_q);

    base::RunLoop loop;

    device_->GetInlineFrameData(base::BindOnce(
        [](base::OnceClosure quit_closure,
           base::OnceCallback<void(mojom::VRPosePtr)> callback,
           mojom::XRFrameDataPtr ptr) {
          std::move(callback).Run(std::move(ptr->mojo_from_viewer));
          std::move(quit_closure).Run();
        },
        loop.QuitClosure(), std::move(callback)));

    task_environment_.RunUntilIdle();

    // Ensure the pose request callback runs.
    loop.Run();
  }

  void AssertInlineFrameDataAvailable(bool expect_available) {
    if (expect_available) {
      device_->GetInlineFrameData(base::BindOnce(
          [](device::mojom::XRFrameDataPtr data) { EXPECT_TRUE(data); }));
    } else {
      device_->GetInlineFrameData(base::BindOnce(
          [](device::mojom::XRFrameDataPtr data) { EXPECT_FALSE(data); }));
    }
  }

  std::unique_ptr<VROrientationSession> MakeDisplay() {
    mojo::PendingRemote<mojom::XRFrameDataProvider> data_provider;
    mojo::PendingRemote<mojom::XRSessionController> controller;
    return std::make_unique<VROrientationSession>(
        device_.get(), data_provider.InitWithNewPipeAndPassReceiver(),
        controller.InitWithNewPipeAndPassReceiver());
  }

  void TryGetFrameData(VROrientationSession* display, bool expect_null) {
    bool was_called = false;
    auto callback = [](bool expect_null, bool* was_called,
                       mojom::XRFrameDataPtr data) {
      *was_called = true;
      EXPECT_EQ(expect_null, !data);
    };

    static_cast<mojom::XRFrameDataProvider*>(display)->GetFrameData(
        nullptr, base::BindOnce(callback, expect_null, &was_called));

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
  }

  mojom::SensorInitParamsPtr FakeInitParams() {
    auto init_params = mojom::SensorInitParams::New();
    init_params->sensor = std::move(sensor_);
    init_params->default_configuration = PlatformSensorConfiguration(
        SensorTraits<kOrientationSensorType>::kDefaultFrequency);

    init_params->client_receiver = sensor_client_.BindNewPipeAndPassReceiver();

    init_params->memory = mapped_region_.region.Duplicate();

    init_params->buffer_offset = GetBufferOffset();

    return init_params;
  }

  void WriteToBuffer(gfx::Quaternion q) {
    size_t offset = GetBufferOffset();
    CHECK(offset % sizeof(SensorReadingSharedBuffer) == 0u);
    auto buffers =
        mapped_region_.mapping.GetMemoryAsSpan<SensorReadingSharedBuffer>();
    SensorReadingSharedBuffer& buffer =
        buffers[offset / sizeof(SensorReadingSharedBuffer)];

    auto& seqlock = buffer.seqlock.value();
    seqlock.WriteBegin();
    buffer.reading.orientation_quat.x = q.x();
    buffer.reading.orientation_quat.y = q.y();
    buffer.reading.orientation_quat.z = q.z();
    buffer.reading.orientation_quat.w = q.w();
    seqlock.WriteEnd();
  }

  void SetRotation(display::Display::Rotation rotation) {
    fake_screen_->display.set_rotation(rotation);
  }

  // Needed for MakeRequest to work.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<VROrientationDevice> device_;
  std::unique_ptr<FakeXRSensorProvider> fake_sensor_provider_;
  mojo::Remote<mojom::SensorProvider> sensor_provider_;

  // Fake Sensor Init params objects
  std::unique_ptr<FakeOrientationSensor> fake_sensor_;
  mojo::PendingRemote<mojom::Sensor> sensor_;
  base::MappedReadOnlyRegion mapped_region_;
  mojo::Remote<mojom::SensorClient> sensor_client_;

  std::unique_ptr<FakeScreen> fake_screen_;
};

TEST_F(VROrientationDeviceTest, InitializationTest) {
  // Check that without running anything, the device will return not available,
  // without crashing.

  device_ = std::make_unique<VROrientationDevice>(sensor_provider_.get(),
                                                  base::DoNothing());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(device_->IsAvailable());
}

TEST_F(VROrientationDeviceTest, SensorNotAvailableTest) {
  // If the provider calls back with nullptr, there are no sensors available.

  InitializeDevice(nullptr);

  EXPECT_FALSE(device_->IsAvailable());
}

TEST_F(VROrientationDeviceTest, SensorIsAvailableTest) {
  // Tests that with proper params the device initializes without mishap.

  InitializeDevice(FakeInitParams());

  EXPECT_TRUE(device_->IsAvailable());
}

TEST_F(VROrientationDeviceTest, GetOrientationTest) {
  // Tests that OnGetInlineFrameData returns a pose ptr without mishap.

  InitializeDevice(FakeInitParams());

  DeviceReadPose(
      gfx::Quaternion(0, 0, 0, 1),
      base::BindOnce([](mojom::VRPosePtr ptr) { EXPECT_TRUE(ptr); }));
}

TEST_F(VROrientationDeviceTest, OrientationDefaultForwardTest) {
  InitializeDevice(FakeInitParams());

  // Set forward to 0 degrees
  DeviceReadPose(gfx::Quaternion(0, 0, 0, 1),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), -0.707, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 0.707, 0.001);
                 }));

  // Now a 90 degree rotation around x in device space should be default pose in
  // vr space.
  DeviceReadPose(gfx::Quaternion(0.707, 0, 0, 0.707),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 1, 0.001);
                 }));
}

TEST_F(VROrientationDeviceTest, OrientationSetForwardTest) {
  InitializeDevice(FakeInitParams());

  // Hold device upright and rotation 45 degrees to left in device space for
  // setting the forward. With the device upright, this causes the first reading
  // to be the default pose.
  DeviceReadPose(gfx::Quaternion(0.653, 0.271, 0.271, 0.653),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 1, 0.001);
                 }));

  // Now hold upright and straigt produces a 45 degree rotation to the right
  DeviceReadPose(gfx::Quaternion(0.707, 0, 0, 0.707),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), -0.383, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 0.924, 0.001);
                 }));
}

TEST_F(VROrientationDeviceTest, OrientationLandscape90Test) {
  InitializeDevice(FakeInitParams());

  SetRotation(display::Display::ROTATE_90);

  // Tilting the device up and twisting to the side should be default in
  // landscape mode.
  DeviceReadPose(gfx::Quaternion(0.5, -0.5, 0.5, 0.5),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 1, 0.001);
                 }));

  // Rotating the device 45 left from base pose should cause 45 degree left
  // rotation around y in VR space.
  DeviceReadPose(gfx::Quaternion(0.653, -0.271, 0.653, 0.271),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), 0.382, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 0.924, 0.001);
                 }));
}

TEST_F(VROrientationDeviceTest, OrientationLandscape270Test) {
  SetRotation(display::Display::ROTATE_270);

  InitializeDevice(FakeInitParams());

  // Tilting the device up and twisting to the side should be default in
  // landscape mode (twist the other way from what we'd need for ROTATE_90).
  DeviceReadPose(gfx::Quaternion(0.5, 0.5, -0.5, 0.5),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 1, 0.001);
                 }));

  // Rotating the device 45 left from base pose should cause 45 degree left
  // rotation around y in VR space
  DeviceReadPose(gfx::Quaternion(0.271, 0.653, -0.271, 0.653),
                 base::BindOnce([](mojom::VRPosePtr ptr) {
                   EXPECT_NEAR(ptr->orientation->x(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->y(), 0.382, 0.001);
                   EXPECT_NEAR(ptr->orientation->z(), 0, 0.001);
                   EXPECT_NEAR(ptr->orientation->w(), 0.924, 0.001);
                 }));
}

TEST_F(VROrientationDeviceTest, GetFrameDataHelper) {
  InitializeDevice(FakeInitParams());

  // 1) create display impl with restricted frame data
  // 2) call GetFrameData and check behavior
  // 3) unrestrict frame data
  // 4) call GetFrameData and check behavior
  std::unique_ptr<VROrientationSession> display = MakeDisplay();
  TryGetFrameData(display.get(), true);
  static_cast<mojom::XRSessionController*>(display.get())
      ->SetFrameDataRestricted(false);
  TryGetFrameData(display.get(), false);
}

}  // namespace device
