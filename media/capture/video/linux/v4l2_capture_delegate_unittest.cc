// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/fcntl.h>
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

using base::test::RunClosure;
using ::testing::_;

namespace media {

namespace {

// Base id and class identifiers for Controls to be modified and later tested
// agains default values.
static struct {
  uint32_t control_base;
  uint32_t class_id;
} const kControls[] = {{V4L2_CID_USER_BASE, V4L2_CID_USER_CLASS},
                       {V4L2_CID_CAMERA_CLASS_BASE, V4L2_CID_CAMERA_CLASS}};

static void SetControlsToMaxValues(int device_fd) {
  v4l2_ext_controls ext_controls;
  memset(&ext_controls, 0, sizeof(ext_controls));
  ext_controls.which = V4L2_CTRL_WHICH_CUR_VAL;
  ext_controls.count = 0;
  const bool use_modern_s_ext_ctrls =
      HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_EXT_CTRLS, &ext_controls)) == 0;

  for (const auto& control : kControls) {
    std::vector<struct v4l2_ext_control> camera_controls;
    std::vector<struct v4l2_ext_control> manual_special_camera_controls;

    v4l2_queryctrl range = {};
    // Start right below the base so that the first next retrieved control ID
    // is always the first available control ID within the class even if that
    // control ID is equal to the base (V4L2_CID_BRIGHTNESS equals to
    // V4L2_CID_USER_BASE).
    range.id = (control.control_base - 1) | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;

      v4l2_ext_control ext_control = {};
      ext_control.id = range.id;

      // Prepare to query for the next control as `range` is an in-out
      // parameter.
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      if (range.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_READ_ONLY)) {
        // Permanently disabled or permanently read-only.
        continue;
      }
      if (V4L2CaptureDelegate::IsBlockedControl(ext_control.id)) {
        continue;
      }

      if (V4L2CaptureDelegate::IsSpecialControl(ext_control.id)) {
        if (ext_control.id == V4L2_CID_EXPOSURE_AUTO) {
          ext_control.value = V4L2_EXPOSURE_MANUAL;
        } else {
          ext_control.value = false;  // Not automatic but manual.
        }
        manual_special_camera_controls.push_back(ext_control);
        DVLOG(1) << __func__ << " " << range.name << " set to manual";
      } else {
        ext_control.value = range.maximum;
        camera_controls.push_back(ext_control);
        DVLOG(1) << __func__ << " " << range.name << " set to "
                 << range.maximum;
      }
    }

    // Set special controls to manual modes.
    if (!manual_special_camera_controls.empty()) {
      ext_controls.which =
          use_modern_s_ext_ctrls ? V4L2_CTRL_WHICH_CUR_VAL : control.class_id;
      ext_controls.count = manual_special_camera_controls.size();
      ext_controls.controls = manual_special_camera_controls.data();
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_EXT_CTRLS, &ext_controls)) <
          0) {
        DPLOG(ERROR) << "VIDIOC_S_EXT_CTRLS";
      }
    }

    // Set non-special controls to maximum values.
    if (!camera_controls.empty()) {
      ext_controls.which =
          use_modern_s_ext_ctrls ? V4L2_CTRL_WHICH_CUR_VAL : control.class_id;
      ext_controls.count = camera_controls.size();
      ext_controls.controls = camera_controls.data();
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_S_EXT_CTRLS, &ext_controls)) < 0)
        DPLOG(ERROR) << "VIDIOC_S_EXT_CTRLS";
    }

    // Start right below the base so that the first next retrieved control ID
    // is always the first available control ID within the class even if that
    // control ID is equal to the base (V4L2_CID_BRIGHTNESS equals to
    // V4L2_CID_USER_BASE).
    range.id = (control.control_base - 1) | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;

      v4l2_control readback = {};
      readback.id = range.id;

      // Prepare to query for the next control as `range` is an in-out
      // parameter.
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      if (range.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_READ_ONLY)) {
        // Permanently disabled or permanently read-only.
        continue;
      }
      if (V4L2CaptureDelegate::IsBlockedControl(readback.id) ||
          V4L2CaptureDelegate::IsSpecialControl(readback.id)) {
        continue;
      }

      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_G_CTRL, &readback)) < 0)
        DPLOG(ERROR) << range.name << ", failed to be read.";
      EXPECT_EQ(range.maximum, readback.value)
          << " control " << range.name << " didn't set correctly";
    }
  }
}

static void VerifyUserControlsAreSetToDefaultValues(int device_fd) {
  for (const auto& control : kControls) {
    v4l2_queryctrl range = {};
    range.id = control.control_base | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == HANDLE_EINTR(ioctl(device_fd, VIDIOC_QUERYCTRL, &range))) {
      if (V4L2_CTRL_ID2CLASS(range.id) != V4L2_CTRL_ID2CLASS(control.class_id))
        break;
      range.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

      DVLOG(1) << __func__ << " " << range.name << ": " << range.minimum << "-"
               << range.maximum << ", default: " << range.default_value;

      v4l2_control current = {};
      current.id = range.id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
      if (HANDLE_EINTR(ioctl(device_fd, VIDIOC_G_CTRL, &current)) < 0)
        DPLOG(ERROR) << "control " << range.name;

      EXPECT_EQ(range.default_value, current.value);
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

}  // anonymous namespace

TEST_F(V4L2CaptureDelegateTest, CreateAndDestroyAndVerifyControls) {
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
    EXPECT_CALL(*client_ptr, OnIncomingCapturedData(_, _, _, _, _, _, _, _, _))
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

}  // namespace media
