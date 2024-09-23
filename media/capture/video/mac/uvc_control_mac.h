// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_UVC_CONTROL_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_UVC_CONTROL_MAC_H_

#import <Foundation/Foundation.h>
#include <IOKit/usb/IOUSBLib.h>

#include <string_view>

#include "base/check.h"
#include "base/logging.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/trace_event/trace_event.h"
#include "media/capture/capture_export.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

using ScopedIOUSBInterfaceInterface =
    base::mac::ScopedIOPluginInterface<IOUSBInterfaceInterface220>;

namespace media {

// In device identifiers, the USB VID and PID are stored in 4 bytes each.
const size_t kVidPidSize = 4;

namespace uvc {

// The following constants are extracted from the specification "Universal
// Serial Bus Device Class Definition for Video Devices", Rev. 1.1 June 1, 2005.
// http://www.usb.org/developers/devclass_docs/USB_Video_Class_1_1.zip
// Sec. A.4 "Video Class-Specific Descriptor Types".
const int kVcCsInterface = 0x24;  // CS_INTERFACE
// Sec. A.5 "Video Class-Specific VC Interface Descriptor Subtypes".
const int kVcInputTerminal = 0x2;   // VC_INPUT_TERMINAL
const int kVcProcessingUnit = 0x5;  // VC_PROCESSING_UNIT
// Sec. A.8 "Video Class-Specific Request Codes".
const int kVcRequestCodeSetCur = 0x1;   // SET_CUR
const int kVcRequestCodeGetCur = 0x81;  // GET_CUR
const int kVcRequestCodeGetMin = 0x82;  // GET_MIN
const int kVcRequestCodeGetMax = 0x83;  // GET_MAX
const int kVcRequestCodeGetRes = 0x84;  // GET_RES
// Sec. A.9.4. "Camera Terminal Control Selectors".
const int kCtAutoExposureModeControl = 0x02;  // CT_AE_MODE_CONTROL
const int kCtExposureTimeAbsoluteControl =
    0x04;                                  // CT_EXPOSURE_TIME_ABSOLUTE_CONTROL
const int kCtFocusAbsoluteControl = 0x06;  // CT_FOCUS_ABSOLUTE_CONTROL
const int kCtFocusAutoControl = 0x08;      // CT_FOCUS_AUTO_CONTROL
const int kCtZoomAbsoluteControl = 0x0b;   // CT_ZOOM_ABSOLUTE_CONTROL
const int kCtPanTiltAbsoluteControl = 0x0d;  // CT_PANTILT_ABSOLUTE_CONTROL
// Sec. A.9.5 "Processing Unit Control Selectors".
const int kPuBrightnessAbsoluteControl = 0x02;  // PU_BRIGHTNESS_CONTROL
const int kPuContrastAbsoluteControl = 0x03;    // PU_CONTRAST_CONTROL
const int kPuPowerLineFrequencyControl =
    0x5;  // PU_POWER_LINE_FREQUENCY_CONTROL
const int kPuSaturationAbsoluteControl = 0x07;  // PU_SATURATION_CONTROL
const int kPuSharpnessAbsoluteControl = 0x08;   // PU_SHARPNESS_CONTROL
const int kPuWhiteBalanceTemperatureControl =
    0x0a;  // PU_WHITE_BALANCE_TEMPERATURE_CONTROL
const int kPuWhiteBalanceTemperatureAutoControl =
    0x0b;  // PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL
// Sec. 4.2.2.1.2 "Auto-Exposure Mode Control".
const int kExposureManual = 1;
const int kExposureShutterPriority = 4;
const int kExposureAperturePriority = 8;
// Sec. 4.2.2.3.5 "Power Line Frequency Control".
const int k50Hz = 1;
const int k60Hz = 2;

}  // namespace uvc

// This utility class handles requests used to manipulate the video controls
// that a USB device exposes through its VideoControl interface and Units
// contained within it.
class CAPTURE_EXPORT UvcControl {
 public:
  explicit UvcControl(std::string device_model, int descriptor_subtype);
  ~UvcControl();

  bool Good() const { return !!interface_; }

  template <typename ValueType>
  bool GetControlCurrent(int control_selector,
                         ValueType* control_current,
                         std::string_view control_name) const {
    return SendControlRequest<ValueType>(uvc::kVcRequestCodeGetCur,
                                         control_selector, control_current,
                                         control_name);
  }

  template <typename ValueType>
  bool GetControlMin(int control_selector,
                     ValueType* control_min,
                     std::string_view control_name) const {
    return SendControlRequest<ValueType>(
        uvc::kVcRequestCodeGetMin, control_selector, control_min, control_name);
  }

  template <typename ValueType>
  bool GetControlMax(int control_selector,
                     ValueType* control_max,
                     std::string_view control_name) const {
    return SendControlRequest<ValueType>(
        uvc::kVcRequestCodeGetMax, control_selector, control_max, control_name);
  }

  template <typename ValueType>
  bool GetControlStep(int control_selector,
                      ValueType* control_step,
                      std::string_view control_name) const {
    return SendControlRequest<ValueType>(uvc::kVcRequestCodeGetRes,
                                         control_selector, control_step,
                                         control_name);
  }

  // Update the control range and current value of control selector if possible.
  template <typename ValueType>
  void MaybeUpdateControlRange(int control_selector,
                               media::mojom::Range* control_range,
                               std::string_view control_name) const {
    ValueType max, min, step, current;
    if (!GetControlMax<ValueType>(control_selector, &max, control_name) ||
        !GetControlMin<ValueType>(control_selector, &min, control_name) ||
        !GetControlStep<ValueType>(control_selector, &step, control_name) ||
        !GetControlCurrent<ValueType>(control_selector, &current,
                                      control_name)) {
      return;
    }
    control_range->max = max;
    control_range->min = min;
    control_range->step = step;
    control_range->current = current;
  }

  template <typename ValueType>
  void SetControlCurrent(int control_selector,
                         ValueType value,
                         std::string_view control_name) const {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                 "UvcControl::SetControlCurrent", "control_name", control_name);
    CHECK(interface_);
    if (!IsControlAvailable(control_selector)) {
      return;
    }
    IOUSBDevRequestTO command =
        CreateEmptyCommand(uvc::kVcRequestCodeSetCur, kUSBOut, control_selector,
                           sizeof(ValueType));
    command.pData = &value;

    IOReturn ret =
        (*interface_.get())->ControlRequestTO(interface_.get(), 0, &command);
    VLOG_IF(1, ret != kIOReturnSuccess)
        << "Set " << control_name << " value to " << value << " failed (0x"
        << std::hex << ret << ")";
    VLOG_IF(1, ret == kIOReturnSuccess) << control_name << " set to " << value;
  }

  static void SetPowerLineFrequency(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      const VideoCaptureParams& params);
  static void GetPhotoState(
      media::mojom::PhotoStatePtr& photo_state,
      const VideoCaptureDeviceDescriptor& device_descriptor);
  static void SetPhotoState(
      mojom::PhotoSettingsPtr& settings,
      const VideoCaptureDeviceDescriptor& device_descriptor);
  static VideoCaptureControlSupport GetControlSupport(
      const std::string& device_model);
  static std::string GetDeviceModelId(const std::string& device_id,
                                      VideoCaptureApi capture_api,
                                      VideoCaptureTransportType transport_type);

 private:
  template <typename ValueType>
  bool SendControlRequest(int request_code,
                          int control_selector,
                          ValueType* result,
                          std::string_view control_name) const {
    TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                 "UvcControl::SendControlRequest", "request_code", request_code,
                 "control_name", control_name);
    CHECK(interface_);
    if (!IsControlAvailable(control_selector)) {
      return false;
    }
    IOUSBDevRequestTO command = CreateEmptyCommand(
        request_code, kUSBIn, control_selector, sizeof(ValueType));
    ValueType data;
    command.pData = &data;

    IOReturn ret =
        (*interface_.get())->ControlRequestTO(interface_.get(), 0, &command);
    VLOG_IF(1, ret != kIOReturnSuccess)
        << control_name << " failed (0x" << std::hex << ret;
    if (ret != kIOReturnSuccess) {
      return false;
    }

    *result = data;
    return true;
  }

  // Returns whether a control is available based on the bmControls bit-map from
  // the descriptor.
  bool IsControlAvailable(int control_selector) const;

  // Create an empty IOUSBDevRequestTO for a USB device to either set or get
  // controls.
  IOUSBDevRequestTO CreateEmptyCommand(int request_code,
                                       int endpoint_direction,
                                       int control_selector,
                                       int control_command_size) const;

  int descriptor_subtype_;
  ScopedIOUSBInterfaceInterface interface_;
  int unit_id_;
  std::vector<uint8_t> controls_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_UVC_CONTROL_MAC_H_
