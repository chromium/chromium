// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/v4l2_webcam.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"

#define V4L2_CID_PAN_SPEED (V4L2_CID_CAMERA_CLASS_BASE+32)
#define V4L2_CID_TILT_SPEED (V4L2_CID_CAMERA_CLASS_BASE+33)
#define V4L2_CID_PANTILT_CMD (V4L2_CID_CAMERA_CLASS_BASE+34)

// GUID of the Extension Unit for Logitech CC3300e motor control:
// {212de5ff-3080-2c4e-82d9-f587d00540bd}
#define UVC_GUID_LOGITECH_CC3000E_MOTORS                                    \
  {                                                                         \
    0x21, 0x2d, 0xe5, 0xff, 0x30, 0x80, 0x2c, 0x4e, 0x82, 0xd9, 0xf5, 0x87, \
        0xd0, 0x05, 0x40, 0xbd                                              \
  }

// GUID of the Extension Unit for AVER XU1 motor control:
// {cb666936-6e26-a047-8451-96ecf60330d6}
#define UVC_GUID_AVER                                                       \
  {                                                                         \
    0xcb, 0x66, 0x69, 0x36, 0x6e, 0x26, 0xa0, 0x47, 0x84, 0x51, 0x96, 0xec, \
        0xf6, 0x03, 0x30, 0xd6                                              \
  }

#define LOGITECH_MOTORCONTROL_PANTILT_CMD 2
#define AVER_UVCX_UCAM_PRESET 0x12

namespace {
const int kLogitechMenuIndexGoHome = 2;
const int kAverMenuIndexGoHome = 1;

const uvc_menu_info kLogitechCmdMenu[] = {
  {1, "Set Preset"}, {2, "Get Preset"}, {3, "Go Home"}
};

// Preset 0 is equivalent to HOME in Aver cameras.
const uvc_menu_info kAverPresetMenu[] = {
    /*0*/ {0x0000, "Set Preset 0"},  /*1*/ {0x0001, "Restore preset 0"},
    /*2*/ {0x0100, "Set Preset 1"},  /*3*/ {0x0101, "Restore preset 1"},
    /*4*/ {0x0200, "Set Preset 2"},  /*5*/ {0x0202, "Restore preset 2"},
    /*6*/ {0x0300, "Set Preset 3"},  /*7*/ {0x0303, "Restore preset 3"},
    /*8*/ {0x0400, "Set Preset 4"},  /*9*/ {0x0404, "Restore preset 4"},
    /*10*/ {0x0500, "Set Preset 5"}, /*11*/ {0x0505, "Restore preset 5"},
    /*12*/ {0x0600, "Set Preset 6"}, /*13*/ {0x0606, "Restore preset 6"},
    /*14*/ {0x0700, "Set Preset 7"}, /*15*/ {0x0707, "Restore preset 7"},
    /*16*/ {0x0800, "Set Preset 8"}, /*17*/ {0x0808, "Restore preset 8"},
    /*18*/ {0x0900, "Set Preset 9"}, /*19*/ {0x0909, "Restore preset 9"},
};

const uvc_xu_control_mapping kLogitechCmdMapping = {
    V4L2_CID_PANTILT_CMD,
    "Pan/Tilt Go",
    UVC_GUID_LOGITECH_CC3000E_MOTORS,
    LOGITECH_MOTORCONTROL_PANTILT_CMD,
    8,
    0,
    V4L2_CTRL_TYPE_MENU,
    UVC_CTRL_DATA_TYPE_ENUM,
    const_cast<uvc_menu_info*>(&kLogitechCmdMenu[0]),
    base::size(kLogitechCmdMenu),
};

const uvc_xu_control_mapping kLogitechPanAbsoluteMapping = {
    V4L2_CID_PAN_ABSOLUTE,
    "Pan (Absolute)",
    UVC_GUID_LOGITECH_CC3000E_MOTORS,
    12,
    32,
    0,
    V4L2_CTRL_TYPE_INTEGER,
    UVC_CTRL_DATA_TYPE_SIGNED,
};

const uvc_xu_control_mapping kLogitechTiltAbsoluteMapping = {
    V4L2_CID_TILT_ABSOLUTE,
    "Tilt (Absolute)",
    UVC_GUID_LOGITECH_CC3000E_MOTORS,
    12,
    32,
    32,
    V4L2_CTRL_TYPE_INTEGER,
    UVC_CTRL_DATA_TYPE_SIGNED,
};

const uvc_xu_control_mapping kAverCmdMapping = {
    V4L2_CID_PANTILT_CMD,
    "PTZ Preset",
    UVC_GUID_AVER,
    AVER_UVCX_UCAM_PRESET,
    24,
    0,
    V4L2_CTRL_TYPE_MENU,
    UVC_CTRL_DATA_TYPE_ENUM,
    const_cast<uvc_menu_info*>(kAverPresetMenu),
    base::size(kAverPresetMenu),
};

}  // namespace

namespace extensions {

V4L2Webcam::V4L2Webcam(const std::string& device_id) : device_id_(device_id) {
}

V4L2Webcam::~V4L2Webcam() {
}

bool V4L2Webcam::Open() {
  fd_.reset(HANDLE_EINTR(open(device_id_.c_str(), 0)));
  return fd_.is_valid();
}

bool V4L2Webcam::EnsureLogitechCommandsMapped() {
  int res =
      HANDLE_EINTR(ioctl(fd_.get(), UVCIOC_CTRL_MAP, &kLogitechCmdMapping));
  // If mapping is successful or it's already mapped, this is a Logitech
  // camera.
  // NOTE: On success, occasionally EFAULT is returned.  On a real error,
  // ENOMEM, EPERM, EINVAL, or EOVERFLOW should be returned.
  return res >= 0 || errno == EEXIST || errno == EFAULT;
}

bool V4L2Webcam::EnsureAverCommandsMapped() {
  int res = HANDLE_EINTR(ioctl(fd_.get(), UVCIOC_CTRL_MAP, &kAverCmdMapping));
  // If mapping is successful or it's already mapped, this is an Aver camera.
  // NOTE: On success, occasionally EFAULT is returned.  On a real error,
  // ENOMEM, EPERM, EINVAL, or EOVERFLOW should be returned.
  return res >= 0 || errno == EEXIST || errno == EFAULT;
}

bool V4L2Webcam::SetWebcamParameter(int fd, uint32_t control_id, int value) {
  // Try to map the V4L2 control to the Logitech extension unit. If the
  // connected camera does not implement these extension unit this will just
  // silently fail and the standard camera terminal controls will be used.
  if (control_id == V4L2_CID_PAN_ABSOLUTE) {
    HANDLE_EINTR(ioctl(fd, UVCIOC_CTRL_MAP, &kLogitechPanAbsoluteMapping));
  } else if (control_id == V4L2_CID_TILT_ABSOLUTE) {
    HANDLE_EINTR(ioctl(fd, UVCIOC_CTRL_MAP, &kLogitechTiltAbsoluteMapping));
  }

  struct v4l2_control v4l2_ctrl = {control_id, value};
  int res = HANDLE_EINTR(ioctl(fd, VIDIOC_S_CTRL, &v4l2_ctrl)) >= 0;
  return res >= 0;
}

bool V4L2Webcam::GetWebcamParameter(int fd,
                                    uint32_t control_id,
                                    int* value,
                                    int* min_value,
                                    int* max_value) {
  // Try to query current value for |control_id|. The getter fails if not
  // supported.
  struct v4l2_control v4l2_ctrl = {control_id};
  if (HANDLE_EINTR(ioctl(fd, VIDIOC_G_CTRL, &v4l2_ctrl)))
    return false;
  *value = v4l2_ctrl.value;

  // Try to query the valid range for |control_id|. Not supporting a range query
  // is not a failure.
  struct v4l2_queryctrl v4l2_range_query = {control_id};
  if (HANDLE_EINTR(ioctl(fd, VIDIOC_QUERYCTRL, &v4l2_range_query))) {
    *min_value = 0;
    *max_value = 0;
  } else {
    *min_value = v4l2_range_query.minimum;
    *max_value = v4l2_range_query.maximum;
  }

  return true;
}

void V4L2Webcam::GetPan(const GetPTZCompleteCallback& callback) {
  int value = 0;
  int min_value = 0;
  int max_value = 0;
  bool success = GetWebcamParameter(fd_.get(), V4L2_CID_PAN_ABSOLUTE, &value,
                                    &min_value, &max_value);

  callback.Run(success, value, min_value, max_value);
}

void V4L2Webcam::GetTilt(const GetPTZCompleteCallback& callback) {
  int value = 0;
  int min_value = 0;
  int max_value = 0;
  bool success = GetWebcamParameter(fd_.get(), V4L2_CID_TILT_ABSOLUTE, &value,
                                    &min_value, &max_value);
  callback.Run(success, value, min_value, max_value);
}

void V4L2Webcam::GetZoom(const GetPTZCompleteCallback& callback) {
  int value = 0;
  int min_value = 0;
  int max_value = 0;
  bool success = GetWebcamParameter(fd_.get(), V4L2_CID_ZOOM_ABSOLUTE, &value,
                                    &min_value, &max_value);
  callback.Run(success, value, min_value, max_value);
}

void V4L2Webcam::GetFocus(const GetPTZCompleteCallback& callback) {
  int value = 0;
  int min_value = 0;
  int max_value = 0;
  bool success = GetWebcamParameter(fd_.get(), V4L2_CID_FOCUS_ABSOLUTE, &value,
                                    &min_value, &max_value);
  callback.Run(success, value, min_value, max_value);
}

void V4L2Webcam::SetPan(int value,
                        int pan_speed,
                        const SetPTZCompleteCallback& callback) {
  callback.Run(SetWebcamParameter(fd_.get(), V4L2_CID_PAN_ABSOLUTE, value));
}

void V4L2Webcam::SetTilt(int value,
                         int tilt_speed,
                         const SetPTZCompleteCallback& callback) {
  callback.Run(SetWebcamParameter(fd_.get(), V4L2_CID_TILT_ABSOLUTE, value));
}

void V4L2Webcam::SetZoom(int value, const SetPTZCompleteCallback& callback) {
  callback.Run(SetWebcamParameter(fd_.get(), V4L2_CID_ZOOM_ABSOLUTE, value));
}

void V4L2Webcam::SetFocus(int value, const SetPTZCompleteCallback& callback) {
  callback.Run(SetWebcamParameter(fd_.get(), V4L2_CID_FOCUS_ABSOLUTE, value));
}

void V4L2Webcam::SetAutofocusState(AutofocusState state,
                                   const SetPTZCompleteCallback& callback) {
  const int value = (state == AUTOFOCUS_ON) ? 1 : 0;
  callback.Run(SetWebcamParameter(fd_.get(), V4L2_CID_FOCUS_AUTO, value));
}

void V4L2Webcam::SetPanDirection(PanDirection direction,
                                 int pan_speed,
                                 const SetPTZCompleteCallback& callback) {
  int direction_value = 0;
  switch (direction) {
    case PAN_STOP:
      direction_value = 0;
      break;

    case PAN_RIGHT:
      direction_value = 1;
      break;

    case PAN_LEFT:
      direction_value = -1;
      break;
  }
  callback.Run(
      SetWebcamParameter(fd_.get(), V4L2_CID_PAN_SPEED, direction_value));
}

void V4L2Webcam::SetTiltDirection(TiltDirection direction,
                                  int tilt_speed,
                                  const SetPTZCompleteCallback& callback) {
  int direction_value = 0;
  switch (direction) {
    case TILT_STOP:
      direction_value = 0;
      break;

    case TILT_UP:
      direction_value = 1;
      break;

    case TILT_DOWN:
      direction_value = -1;
      break;
  }
  callback.Run(
      SetWebcamParameter(fd_.get(), V4L2_CID_TILT_SPEED, direction_value));
}

void V4L2Webcam::Reset(bool pan,
                       bool tilt,
                       bool zoom,
                       const SetPTZCompleteCallback& callback) {
  if (pan || tilt) {
    if (EnsureLogitechCommandsMapped()) {
      if (!SetWebcamParameter(fd_.get(), V4L2_CID_PANTILT_CMD,
                              kLogitechMenuIndexGoHome)) {
        callback.Run(false);
        return;
      }
    } else if (EnsureAverCommandsMapped()) {
      if (!SetWebcamParameter(fd_.get(), V4L2_CID_PANTILT_CMD,
                              kAverMenuIndexGoHome)) {
        callback.Run(false);
        return;
      }
    } else {
      if (pan) {
        struct v4l2_control v4l2_ctrl = {V4L2_CID_PAN_RESET};
        if (!HANDLE_EINTR(ioctl(fd_.get(), VIDIOC_S_CTRL, &v4l2_ctrl))) {
          callback.Run(false);
          return;
        }
      }

      if (tilt) {
        struct v4l2_control v4l2_ctrl = {V4L2_CID_TILT_RESET};
        if (!HANDLE_EINTR(ioctl(fd_.get(), VIDIOC_S_CTRL, &v4l2_ctrl))) {
          callback.Run(false);
          return;
        }
      }
    }
  }

  if (zoom) {
    const int kDefaultZoom = 100;
    if (!SetWebcamParameter(fd_.get(), V4L2_CID_ZOOM_ABSOLUTE, kDefaultZoom)) {
      callback.Run(false);
      return;
    }
  }

  callback.Run(true);
}

}  // namespace extensions
