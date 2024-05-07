// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/ioctl.h>

#include "base/files/file_enumerator.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/capture/video/linux/v4l2_capture_delegate.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX)
#include "base/command_line.h"
#include "media/capture/capture_switches.h"
#include "media/capture/video/linux/fake_device_provider.h"
#include "media/capture/video/linux/fake_v4l2_impl.h"
#include "media/capture/video/linux/video_capture_device_linux.h"
#include "media/video/fake_gpu_memory_buffer.h"
#endif  // BUILDFLAG(IS_LINUX)

using base::test::RunClosure;
using ::testing::_;

#if BUILDFLAG(IS_LINUX)
using testing::Invoke;
using testing::InvokeWithoutArgs;
#endif  // BUILDFLAG(IS_LINUX)

namespace media {

namespace {

#if BUILDFLAG(IS_LINUX)
constexpr int kFrameToReceive = 3;
#endif  // BUILDFLAG(IS_LINUX)

// Base id and class identifiers for Controls to be modified and later tested
// against default values.
static struct {
  uint32_t control_base;
  uint32_t class_id;
} const kControls[] = {{V4L2_CID_USER_BASE, V4L2_CID_USER_CLASS},
                       {V4L2_CID_CAMERA_CLASS_BASE, V4L2_CID_CAMERA_CLASS}};

// Determines if |control_id| is special, i.e. controls another one's state, or
// if it should be denied (see https://crbug.com/697885).
#if !defined(V4L2_CID_PAN_SPEED)
#define V4L2_CID_PAN_SPEED (V4L2_CID_CAMERA_CLASS_BASE + 32)
#endif
#if !defined(V4L2_CID_TILT_SPEED)
#define V4L2_CID_TILT_SPEED (V4L2_CID_CAMERA_CLASS_BASE + 33)
#endif
#if !defined(V4L2_CID_PANTILT_CMD)
#define V4L2_CID_PANTILT_CMD (V4L2_CID_CAMERA_CLASS_BASE + 34)
#endif
static bool IsSpecialOrBlockedControl(int control_id) {
  switch (control_id) {
    case V4L2_CID_AUTO_WHITE_BALANCE:
    case V4L2_CID_EXPOSURE_AUTO:
    case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
    case V4L2_CID_FOCUS_AUTO:
    case V4L2_CID_PAN_RELATIVE:
    case V4L2_CID_TILT_RELATIVE:
    case V4L2_CID_PAN_RESET:
    case V4L2_CID_TILT_RESET:
    case V4L2_CID_PAN_ABSOLUTE:
    case V4L2_CID_TILT_ABSOLUTE:
    case V4L2_CID_ZOOM_ABSOLUTE:
    case V4L2_CID_ZOOM_RELATIVE:
    case V4L2_CID_ZOOM_CONTINUOUS:
    case V4L2_CID_PAN_SPEED:
    case V4L2_CID_TILT_SPEED:
    case V4L2_CID_PANTILT_CMD:
      return true;
  }
  return false;
}

static void SetControlsToMaxValues(int device_fd) {
  // Set V4L2_CID_AUTO_WHITE_BALANCE to false first.
  v4l2_control auto_white_balance = {};
  auto_white_balance.id = V4L2_CID_AUTO_WHITE_BALANCE;
  auto_white_balance.value = false;
  if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_CTRL, &auto_white_balance)) < 0)
    DPLOG(ERROR) << "VIDIOC_S_CTRL";

  std::vector<struct v4l2_ext_control> special_camera_controls;
  // Set V4L2_CID_EXPOSURE_AUTO to V4L2_EXPOSURE_MANUAL.
  v4l2_ext_control auto_exposure = {};
  auto_exposure.id = V4L2_CID_EXPOSURE_AUTO;
  auto_exposure.value = V4L2_EXPOSURE_MANUAL;
  special_camera_controls.push_back(auto_exposure);
  // Set V4L2_CID_EXPOSURE_AUTO_PRIORITY to false.
  v4l2_ext_control priority_auto_exposure = {};
  priority_auto_exposure.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
  priority_auto_exposure.value = false;
  special_camera_controls.push_back(priority_auto_exposure);
  // Set V4L2_CID_FOCUS_AUTO to false.
  v4l2_ext_control auto_focus = {};
  auto_focus.id = V4L2_CID_FOCUS_AUTO;
  auto_focus.value = false;
  special_camera_controls.push_back(auto_focus);

  struct v4l2_ext_controls camera_ext_controls = {};
  camera_ext_controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
  camera_ext_controls.count = special_camera_controls.size();
  camera_ext_controls.controls = special_camera_controls.data();
  if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_EXT_CTRLS, &camera_ext_controls)) <
      0) {
    DPLOG(ERROR) << "VIDIOC_S_EXT_CTRLS";
  }

  for (const auto& control : kControls) {
    std::vector<struct v4l2_ext_control> camera_controls;

    v4l2_queryctrl range = {};
    range.id = control.control_base | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      if (IsSpecialOrBlockedControl(range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL))
        continue;
      DVLOG(1) << __func__ << " " << range.name << " set to " << range.maximum;

      struct v4l2_ext_control ext_control = {};
      ext_control.id = range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
      ext_control.value = range.maximum;
      camera_controls.push_back(ext_control);
    }

    if (!camera_controls.empty()) {
      struct v4l2_ext_controls ext_controls = {};
      ext_controls.ctrl_class = control.class_id;
      ext_controls.count = camera_controls.size();
      ext_controls.controls = camera_controls.data();
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_EXT_CTRLS, &ext_controls)) < 0)
        DPLOG(ERROR) << "VIDIOC_S_EXT_CTRLS";
    }

    range.id = control.control_base | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      if (IsSpecialOrBlockedControl(range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL))
        continue;
      DVLOG(1) << __func__ << " " << range.name << " set to " << range.maximum;

      v4l2_control readback = {};
      readback.id = range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_G_CTRL, &readback)) < 0)
        DPLOG(ERROR) << range.name << ", failed to be read.";
      EXPECT_EQ(range.maximum, readback.value)
          << " control " << range.name << " didn't set correctly";
    }
  }
}

static void VerifyUserControlsAreSetToDefaultValues(int device_fd) {
  auto do_ioctl = base::BindRepeating(
      [](int device_fd, int request, void* argp) {
        return HANDLE_EINTR(ioctl(device_fd, request, argp));
      },
      device_fd);

  for (const auto& control : kControls) {
    v4l2_queryctrl range = {};
    // Start right below the base so that the first next retrieved control ID
    // is always the first available control ID within the class even if that
    // control ID is equal to the base (V4L2_CID_BRIGHTNESS equals to
    // V4L2_CID_USER_BASE).
    range.id = (control.control_base - 1) | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;

      DVLOG(1) << __func__ << " " << range.name << ": " << range.minimum << "-"
               << range.maximum << ", default: " << range.default_value;

      v4l2_control current = {};
      current.id = range.id;

      // Prepare to query for the next control as `range` is an in-out
      // parameter.
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      if (range.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_READ_ONLY)) {
        // Permanently disabled or permanently read-only.
        continue;
      }
      if (V4L2CaptureDelegate::IsBlockedControl(current.id) ||
          !V4L2CaptureDelegate::IsControllableControl(current.id, do_ioctl)) {
        // Skip controls which are blocked and controls which are controlled
        // by special controls which are in automatic states.
        continue;
      }

      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_G_CTRL, &current)) < 0)
        DPLOG(ERROR) << "control " << range.name;

      EXPECT_EQ(range.default_value, current.value)
          << " control " << range.name << " didn't reset correctly";
    }
  }
}

class V4L2CaptureDelegateTest : public ::testing::Test {
 public:
  V4L2CaptureDelegateTest()
      : device_descriptor_("Device 0", "/dev/video0"),
        v4l2_(new V4L2CaptureDeviceImpl()),
        delegate_(std::make_unique<V4L2CaptureDelegate>(
            v4l2_.get(),
            device_descriptor_,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            50,
            0)) {}
  ~V4L2CaptureDelegateTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;
  VideoCaptureDeviceDescriptor device_descriptor_;
  scoped_refptr<V4L2CaptureDevice> v4l2_;
  std::unique_ptr<V4L2CaptureDelegate> delegate_;
};

#if BUILDFLAG(IS_LINUX)
class MockV4l2GpuClient : public VideoCaptureDevice::Client {
 public:
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& frame_format,
                              const gfx::ColorSpace& color_space,
                              int clockwise_rotation,
                              bool flip_y,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp,
                              std::optional<base::TimeTicks> capture_begin_time,
                              int frame_feedback_id = 0) override {}

  void OnIncomingCapturedGfxBuffer(
      gfx::GpuMemoryBuffer* buffer,
      const VideoCaptureFormat& frame_format,
      int clockwise_rotation,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      int frame_feedback_id = 0) override {}

  void OnIncomingCapturedExternalBuffer(
      CapturedExternalVideoBuffer buffer,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time,
      const gfx::Rect& visible_rect) override {}

  void OnCaptureConfigurationChanged() override {}

  MOCK_METHOD6(ReserveOutputBuffer,
               ReserveResult(const gfx::Size&,
                             VideoPixelFormat,
                             int,
                             Buffer*,
                             int*,
                             int*));

  void OnIncomingCapturedBuffer(
      Buffer buffer,
      const VideoCaptureFormat& format,
      base::TimeTicks reference_,
      base::TimeDelta timestamp,
      std::optional<base::TimeTicks> capture_begin_time) override {}

  MOCK_METHOD8(OnIncomingCapturedBufferExt,
               void(Buffer,
                    const VideoCaptureFormat&,
                    const gfx::ColorSpace&,
                    base::TimeTicks,
                    base::TimeDelta,
                    std::optional<base::TimeTicks>,
                    gfx::Rect,
                    const VideoFrameMetadata&));

  MOCK_METHOD3(OnError,
               void(VideoCaptureError,
                    const base::Location&,
                    const std::string&));

  MOCK_METHOD1(OnFrameDropped, void(VideoCaptureFrameDropReason));

  double GetBufferPoolUtilization() const override { return 0.0; }

  void OnStarted() override {}
};

class MockCaptureHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  MockCaptureHandleProvider(const gfx::Size& size, gfx::BufferFormat format) {
    gmb_ = std::make_unique<FakeGpuMemoryBuffer>(size, format);
  }
  // Duplicate as an writable (unsafe) shared memory region.
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    return base::UnsafeSharedMemoryRegion();
  }

  // Access a |VideoCaptureBufferHandle| for local, writable memory.
  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return nullptr;
  }

  // Clone a |GpuMemoryBufferHandle| for IPC.
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    gfx::GpuMemoryBufferHandle handle;
    return gmb_->CloneHandle();
  }
  std::unique_ptr<FakeGpuMemoryBuffer> gmb_;
};

class V4l2CaptureDelegateGPUMemoryBufferTest
    : public ::testing::TestWithParam<uint32_t> {
 public:
  V4l2CaptureDelegateGPUMemoryBufferTest() = default;
  ~V4l2CaptureDelegateGPUMemoryBufferTest() override = default;

 public:
  void SetUp() override {
    device_factory_ = std::make_unique<VideoCaptureDeviceFactoryV4L2>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    scoped_refptr<FakeV4L2Impl> fake_v4l2(new FakeV4L2Impl());
    fake_v4l2_ = fake_v4l2.get();
    auto fake_device_provider = std::make_unique<FakeDeviceProvider>();
    fake_device_provider_ = fake_device_provider.get();
    device_factory_->SetV4L2EnvironmentForTesting(
        std::move(fake_v4l2), std::move(fake_device_provider));
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kVideoCaptureUseGpuMemoryBuffer);
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoCaptureDeviceFactoryV4L2> device_factory_;
  raw_ptr<FakeV4L2Impl> fake_v4l2_;
  raw_ptr<FakeDeviceProvider> fake_device_provider_;
  int received_frame_count_ = 0;
};
#endif  // BUILDFLAG(IS_LINUX)
}  // anonymous namespace

// Fails on Linux, see crbug/732355
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CreateAndDestroyAndVerifyControls \
  DISABLED_CreateAndDestroyAndVerifyControls
#else
#define MAYBE_CreateAndDestroyAndVerifyControls \
  CreateAndDestroyAndVerifyControls
#endif
TEST_F(V4L2CaptureDelegateTest, MAYBE_CreateAndDestroyAndVerifyControls) {
  // Check that there is at least a video device, otherwise bail.
  const base::FilePath path("/dev/");
  base::FileEnumerator enumerator(path, false, base::FileEnumerator::FILES,
                                  "video*");
  if (enumerator.Next().empty()) {
    DLOG(INFO) << " No devices found, skipping test";
    return;
  }

  // Open device, manipulate user and camera controls, and close it.
  {
    base::ScopedFD device_fd(
        HANDLE_EINTR(open(device_descriptor_.device_id.c_str(), O_RDWR)));
    ASSERT_TRUE(device_fd.is_valid());

    SetControlsToMaxValues(device_fd.get());

    base::RunLoop().RunUntilIdle();
  }

  // Start and stop capturing, triggering the resetting of user and camera
  // control values.
  {
    std::unique_ptr<MockVideoCaptureDeviceClient> client(
        new MockVideoCaptureDeviceClient());
    MockVideoCaptureDeviceClient* client_ptr = client.get();
    EXPECT_CALL(*client_ptr, OnStarted());
    delegate_->AllocateAndStart(320 /* width */, 240 /* height */,
                                10.0 /* frame_rate */, std::move(client));

    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*client_ptr, OnIncomingCapturedData)
        .Times(1)
        .WillOnce(RunClosure(quit_closure));
    run_loop.Run();

    delegate_->StopAndDeAllocate();
    base::RunLoop().RunUntilIdle();
  }

  // Reopen the device and verify all user and camera controls should be back to
  // their |default_value|s.
  {
    base::ScopedFD device_fd(
        HANDLE_EINTR(open(device_descriptor_.device_id.c_str(), O_RDWR)));
    ASSERT_TRUE(device_fd.is_valid());
    VerifyUserControlsAreSetToDefaultValues(device_fd.get());
  }
}

#if BUILDFLAG(IS_LINUX)
TEST_P(V4l2CaptureDelegateGPUMemoryBufferTest, CameraCaptureOneCopy) {
  const std::string stub_display_name("Fake Device 0");
  const std::string stub_device_id("/dev/video0");
  const uint32_t fmt = GetParam();
  VideoCaptureDeviceDescriptor descriptor(
      stub_display_name, stub_device_id,
      VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE);
  fake_device_provider_->AddDevice(descriptor);
  fake_v4l2_->AddDevice(stub_device_id, FakeV4L2DeviceConfig(descriptor, fmt));
  std::unique_ptr<VideoCaptureDevice> device =
      device_factory_->CreateDevice(descriptor).ReleaseDevice();
  auto fake_gmb_support = std::make_unique<FakeGpuMemoryBufferSupport>();
  ((VideoCaptureDeviceLinux*)device.get())
      ->SetGPUEnvironmentForTesting(std::move(fake_gmb_support));
  received_frame_count_ = 0;

  std::unique_ptr<MockV4l2GpuClient> client =
      std::make_unique<MockV4l2GpuClient>();
  MockV4l2GpuClient* client_ptr = client.get();
  EXPECT_CALL(*client_ptr, ReserveOutputBuffer)
      .WillRepeatedly(Invoke(
          [](const gfx::Size& size, VideoPixelFormat format, int feedback_id,
             VideoCaptureDevice::Client::Buffer* capture_buffer,
             int* require_new_buffer_id, int* retire_old_buffer_id) {
            EXPECT_EQ(format, PIXEL_FORMAT_NV12);
            capture_buffer->handle_provider =
                std::make_unique<MockCaptureHandleProvider>(
                    size, gfx::BufferFormat::YUV_420_BIPLANAR);
            return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
          }));

  base::RunLoop wait_loop;
  EXPECT_CALL(*client_ptr, OnIncomingCapturedBufferExt)
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, this]() {
        this->received_frame_count_++;
        if (this->received_frame_count_ == kFrameToReceive) {
          wait_loop.Quit();
        }
      }));

  VideoCaptureParams arbitrary_params;
  arbitrary_params.requested_format.frame_size = gfx::Size(1280, 720);
  arbitrary_params.requested_format.frame_rate = 30.0f;
  arbitrary_params.requested_format.pixel_format = PIXEL_FORMAT_NV21;
  device->AllocateAndStart(arbitrary_params, std::move(client));
  wait_loop.Run();
  device->StopAndDeAllocate();
}

INSTANTIATE_TEST_SUITE_P(All,
                         V4l2CaptureDelegateGPUMemoryBufferTest,
                         ::testing::Values((V4L2_PIX_FMT_NV12),
                                           (V4L2_PIX_FMT_YUYV),
                                           (V4L2_PIX_FMT_MJPEG),
                                           (V4L2_PIX_FMT_RGB24)));
#endif  // BUILDFLAG(IS_LINUX)
}  // namespace media
