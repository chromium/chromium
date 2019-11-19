// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/xbox_controller_mac.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USB.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_uma.h"

namespace device {

namespace {

const int kXbox360ReadEndpoint = 1;
const int kXbox360ControlEndpoint = 2;

const int kXboxOneReadEndpoint = 2;
const int kXboxOneControlEndpoint = 1;

const double kXboxOneMaxEffectDurationMillis = 2500;  // 2.5 seconds

const size_t kXbox360HeaderBytes = 2;
const size_t kXboxOneHeaderBytes = 4;

enum {
  STATUS_MESSAGE_BUTTONS = 0,
  STATUS_MESSAGE_LED = 1,

  // Apparently this message tells you if the rumble pack is disabled in the
  // controller. If the rumble pack is disabled, vibration control messages
  // have no effect.
  STATUS_MESSAGE_RUMBLE = 3,
};

enum {
  XBOX_ONE_STATUS_MESSAGE_BUTTONS = 0x20,
  XBOX_ONE_STATUS_MESSAGE_GUIDE = 0x07
};

enum {
  CONTROL_MESSAGE_SET_RUMBLE = 0,
  CONTROL_MESSAGE_SET_LED = 1,
};

#pragma pack(push, 1)
struct Xbox360ButtonData {
  bool dpad_up : 1;
  bool dpad_down : 1;
  bool dpad_left : 1;
  bool dpad_right : 1;

  bool start : 1;
  bool back : 1;
  bool stick_left_click : 1;
  bool stick_right_click : 1;

  bool bumper_left : 1;
  bool bumper_right : 1;
  bool guide : 1;
  bool dummy1 : 1;  // Always 0.

  bool a : 1;
  bool b : 1;
  bool x : 1;
  bool y : 1;

  uint8_t trigger_left;
  uint8_t trigger_right;

  int16_t stick_left_x;
  int16_t stick_left_y;
  int16_t stick_right_x;
  int16_t stick_right_y;

  // Always 0.
  uint32_t dummy2;
  uint16_t dummy3;
};

struct Xbox360RumbleData {
  uint8_t command;
  uint8_t size;
  uint8_t dummy1;
  uint8_t big;
  uint8_t little;
  uint8_t dummy2[3];
};

struct XboxOneButtonData {
  bool sync : 1;
  bool dummy1 : 1;  // Always 0.
  bool start : 1;
  bool back : 1;

  bool a : 1;
  bool b : 1;
  bool x : 1;
  bool y : 1;

  bool dpad_up : 1;
  bool dpad_down : 1;
  bool dpad_left : 1;
  bool dpad_right : 1;

  bool bumper_left : 1;
  bool bumper_right : 1;
  bool stick_left_click : 1;
  bool stick_right_click : 1;

  uint16_t trigger_left;
  uint16_t trigger_right;

  int16_t stick_left_x;
  int16_t stick_left_y;
  int16_t stick_right_x;
  int16_t stick_right_y;
};

struct XboxOneGuideData {
  uint8_t down;
  uint8_t dummy1;
};

struct XboxOneRumbleData {
  uint8_t command;
  uint8_t dummy1;
  uint8_t counter;
  uint8_t size;
  uint8_t mode;
  uint8_t rumble_mask;
  uint8_t trigger_left;
  uint8_t trigger_right;
  uint8_t strong_magnitude;
  uint8_t weak_magnitude;
  uint8_t duration;
  uint8_t period;
  uint8_t extra;
};
#pragma pack(pop)

static_assert(sizeof(Xbox360ButtonData) == 18, "Xbox360ButtonData wrong size");
static_assert(sizeof(Xbox360RumbleData) == 8, "Xbox360RumbleData wrong size");
static_assert(sizeof(XboxOneButtonData) == 14, "XboxOneButtonData wrong size");
static_assert(sizeof(XboxOneGuideData) == 2, "XboxOneGuideData wrong size");
static_assert(sizeof(XboxOneRumbleData) == 13, "XboxOneRumbleData wrong size");

// Report lengths for the input reports that carry gamepad button and axis data
// on special Xbox One devices. These devices support input remapping and
// include both the mapped and unmapped data in the input report, along with
// additional data specific to the device. This driver only uses the mapped
// data, which is at the beginning of the report and has the same structure as
// the standard XboxOneButtonData report.
const size_t kXboxOneEliteButtonDataBytes = 29;
const size_t kXboxAdaptiveButtonDataBytes = 50;

// From MSDN:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ee417001(v=vs.85).aspx#dead_zone
const int16_t kLeftThumbDeadzone = 7849;
const int16_t kRightThumbDeadzone = 8689;
const uint8_t kXbox360TriggerDeadzone = 30;
const uint16_t kXboxOneTriggerMax = 1023;
const uint16_t kXboxOneTriggerDeadzone = 120;

void NormalizeAxis(int16_t x,
                   int16_t y,
                   int16_t deadzone,
                   float* x_out,
                   float* y_out) {
  float x_val = x;
  float y_val = y;

  // Determine how far the stick is pushed.
  float real_magnitude = std::sqrt(x_val * x_val + y_val * y_val);

  // Check if the controller is outside a circular dead zone.
  if (real_magnitude > deadzone) {
    // Clip the magnitude at its expected maximum value.
    float magnitude = std::min(32767.0f, real_magnitude);

    // Adjust magnitude relative to the end of the dead zone.
    magnitude -= deadzone;

    // Normalize the magnitude with respect to its expected range giving a
    // magnitude value of 0.0 to 1.0
    float ratio = (magnitude / (32767 - deadzone)) / real_magnitude;

    // Y is negated because xbox controllers have an opposite sign from
    // the 'standard controller' recommendations.
    *x_out = x_val * ratio;
    *y_out = -y_val * ratio;
  } else {
    // If the controller is in the deadzone zero out the magnitude.
    *x_out = *y_out = 0.0f;
  }
}

float NormalizeTrigger(uint8_t value) {
  return value < kXbox360TriggerDeadzone
             ? 0
             : static_cast<float>(value - kXbox360TriggerDeadzone) /
                   (std::numeric_limits<uint8_t>::max() -
                    kXbox360TriggerDeadzone);
}

float NormalizeXboxOneTrigger(uint16_t value) {
  return value < kXboxOneTriggerDeadzone
             ? 0
             : static_cast<float>(value - kXboxOneTriggerDeadzone) /
                   (kXboxOneTriggerMax - kXboxOneTriggerDeadzone);
}

void NormalizeXbox360ButtonData(const Xbox360ButtonData& data,
                                XboxControllerMac::Data* normalized_data) {
  normalized_data->buttons[0] = data.a;
  normalized_data->buttons[1] = data.b;
  normalized_data->buttons[2] = data.x;
  normalized_data->buttons[3] = data.y;
  normalized_data->buttons[4] = data.bumper_left;
  normalized_data->buttons[5] = data.bumper_right;
  normalized_data->buttons[6] = data.back;
  normalized_data->buttons[7] = data.start;
  normalized_data->buttons[8] = data.stick_left_click;
  normalized_data->buttons[9] = data.stick_right_click;
  normalized_data->buttons[10] = data.dpad_up;
  normalized_data->buttons[11] = data.dpad_down;
  normalized_data->buttons[12] = data.dpad_left;
  normalized_data->buttons[13] = data.dpad_right;
  normalized_data->buttons[14] = data.guide;
  normalized_data->triggers[0] = NormalizeTrigger(data.trigger_left);
  normalized_data->triggers[1] = NormalizeTrigger(data.trigger_right);
  NormalizeAxis(data.stick_left_x, data.stick_left_y, kLeftThumbDeadzone,
                &normalized_data->axes[0], &normalized_data->axes[1]);
  NormalizeAxis(data.stick_right_x, data.stick_right_y, kRightThumbDeadzone,
                &normalized_data->axes[2], &normalized_data->axes[3]);
}

void NormalizeXboxOneButtonData(const XboxOneButtonData& data,
                                XboxControllerMac::Data* normalized_data) {
  normalized_data->buttons[0] = data.a;
  normalized_data->buttons[1] = data.b;
  normalized_data->buttons[2] = data.x;
  normalized_data->buttons[3] = data.y;
  normalized_data->buttons[4] = data.bumper_left;
  normalized_data->buttons[5] = data.bumper_right;
  normalized_data->buttons[6] = data.back;
  normalized_data->buttons[7] = data.start;
  normalized_data->buttons[8] = data.stick_left_click;
  normalized_data->buttons[9] = data.stick_right_click;
  normalized_data->buttons[10] = data.dpad_up;
  normalized_data->buttons[11] = data.dpad_down;
  normalized_data->buttons[12] = data.dpad_left;
  normalized_data->buttons[13] = data.dpad_right;
  normalized_data->triggers[0] = NormalizeXboxOneTrigger(data.trigger_left);
  normalized_data->triggers[1] = NormalizeXboxOneTrigger(data.trigger_right);
  NormalizeAxis(data.stick_left_x, data.stick_left_y, kLeftThumbDeadzone,
                &normalized_data->axes[0], &normalized_data->axes[1]);
  NormalizeAxis(data.stick_right_x, data.stick_right_y, kRightThumbDeadzone,
                &normalized_data->axes[2], &normalized_data->axes[3]);
}

XboxControllerMac::ControllerType ControllerTypeFromIds(uint16_t vendor_id,
                                                        uint16_t product_id) {
  if (vendor_id == XboxControllerMac::kVendorMicrosoft) {
    switch (product_id) {
      case XboxControllerMac::kProductXbox360Controller:
        return XboxControllerMac::XBOX_360_CONTROLLER;
      case XboxControllerMac::kProductXboxOneController2013:
        return XboxControllerMac::XBOX_ONE_CONTROLLER_2013;
      case XboxControllerMac::kProductXboxOneController2015:
        return XboxControllerMac::XBOX_ONE_CONTROLLER_2015;
      case XboxControllerMac::kProductXboxOneEliteController:
        return XboxControllerMac::XBOX_ONE_ELITE_CONTROLLER;
      case XboxControllerMac::kProductXboxOneSController:
        return XboxControllerMac::XBOX_ONE_S_CONTROLLER;
      case XboxControllerMac::kProductXboxAdaptiveController:
        return XboxControllerMac::XBOX_ADAPTIVE_CONTROLLER;
      default:
        break;
    }
  }
  return XboxControllerMac::UNKNOWN_CONTROLLER;
}

}  // namespace

XboxControllerMac::XboxControllerMac(Delegate* delegate)
    : delegate_(delegate) {}

XboxControllerMac::~XboxControllerMac() = default;

void XboxControllerMac::DoShutdown() {
  if (source_)
    CFRunLoopSourceInvalidate(source_);
  source_.reset();
  if (interface_ && interface_is_open_)
    (*interface_)->USBInterfaceClose(interface_);
  interface_.reset();
  if (device_ && device_is_open_)
    (*device_)->USBDeviceClose(device_);
  device_.reset();
}

double XboxControllerMac::GetMaxEffectDurationMillis() {
  // The Xbox One controller rumble packet specifies a duration for the rumble
  // effect with a maximum length of about 3 seconds.
  return kXboxOneMaxEffectDurationMillis;
}

void XboxControllerMac::SetVibration(double strong_magnitude,
                                     double weak_magnitude) {
  // Clamp magnitudes to [0,1]
  strong_magnitude =
      std::max<double>(0.0, std::min<double>(strong_magnitude, 1.0));
  weak_magnitude = std::max<double>(0.0, std::min<double>(weak_magnitude, 1.0));

  if (controller_type_ == XBOX_360_CONTROLLER) {
    WriteXbox360Rumble(static_cast<uint8_t>(strong_magnitude * 255.0),
                       static_cast<uint8_t>(weak_magnitude * 255.0));
  } else if (controller_type_ == XBOX_ONE_CONTROLLER_2013 ||
             controller_type_ == XBOX_ONE_CONTROLLER_2015 ||
             controller_type_ == XBOX_ONE_ELITE_CONTROLLER ||
             controller_type_ == XBOX_ONE_S_CONTROLLER) {
    WriteXboxOneRumble(static_cast<uint8_t>(strong_magnitude * 255.0),
                       static_cast<uint8_t>(weak_magnitude * 255.0));
  }
}

XboxControllerMac::OpenDeviceResult XboxControllerMac::OpenDevice(
    io_service_t service) {
  IOCFPlugInInterface** plugin;
  SInt32 score;  // Unused, but required for IOCreatePlugInInterfaceForService.
  kern_return_t kr = IOCreatePlugInInterfaceForService(
      service, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin,
      &score);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_ref(plugin);

  HRESULT res = (*plugin)->QueryInterface(
      plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID320),
      (LPVOID*)&device_);
  if (!SUCCEEDED(res) || !device_)
    return OPEN_FAILED;

  uint16_t vendor_id;
  kr = (*device_)->GetDeviceVendor(device_, &vendor_id);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;

  uint16_t product_id;
  kr = (*device_)->GetDeviceProduct(device_, &product_id);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;

  // Record a connected XInput gamepad. Non-XInput devices are recorded
  // elsewhere.
  DCHECK_NE(kXInputTypeNone,
            GamepadIdList::Get().GetXInputType(vendor_id, product_id));
  RecordConnectedGamepad(vendor_id, product_id);

  // Only genuine Microsoft Xbox, Xbox 360, and Xbox One devices are supported.
  if (vendor_id != kVendorMicrosoft)
    return OPEN_FAILED;

  controller_type_ = ControllerTypeFromIds(vendor_id, product_id);

  IOUSBFindInterfaceRequest request;
  switch (controller_type_) {
    case XBOX_360_CONTROLLER:
      read_endpoint_ = kXbox360ReadEndpoint;
      control_endpoint_ = kXbox360ControlEndpoint;
      request.bInterfaceClass = 255;
      request.bInterfaceSubClass = 93;
      request.bInterfaceProtocol = 1;
      request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
      break;
    case XBOX_ONE_CONTROLLER_2013:
    case XBOX_ONE_CONTROLLER_2015:
    case XBOX_ONE_ELITE_CONTROLLER:
    case XBOX_ONE_S_CONTROLLER:
    case XBOX_ADAPTIVE_CONTROLLER:
      read_endpoint_ = kXboxOneReadEndpoint;
      control_endpoint_ = kXboxOneControlEndpoint;
      request.bInterfaceClass = 255;
      request.bInterfaceSubClass = 71;
      request.bInterfaceProtocol = 208;
      request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
      break;
    default:
      return OPEN_FAILED;
  }

  // Open the device and configure it.
  kr = (*device_)->USBDeviceOpen(device_);
  if (kr == kIOReturnExclusiveAccess) {
    // USBDeviceOpen may fail with kIOReturnExclusiveAccess if the device has
    // already been opened by another process. Usually this is temporary and
    // the device will soon become available. Signal to the data fetcher that
    // it should retry.
    return OPEN_FAILED_EXCLUSIVE_ACCESS;
  } else if (kr != KERN_SUCCESS) {
    return OPEN_FAILED;
  }
  device_is_open_ = true;

  // Xbox controllers have one configuration option which has configuration
  // value 1. Try to set it and fail if it couldn't be configured.
  IOUSBConfigurationDescriptorPtr config_desc;
  kr = (*device_)->GetConfigurationDescriptorPtr(device_, 0, &config_desc);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;
  kr = (*device_)->SetConfiguration(device_, config_desc->bConfigurationValue);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;

  // The device has 4 interfaces. They are as follows:
  // Protocol 1:
  //  - Endpoint 1 (in) : Controller events, including button presses.
  //  - Endpoint 2 (out): Rumble pack and LED control
  // Protocol 2 has a single endpoint to read from a connected ChatPad device.
  // Protocol 3 is used by a connected headset device.
  // The device also has an interface on subclass 253, protocol 10 with no
  // endpoints.  It is unused.
  //
  // We don't currently support the ChatPad or headset, so protocol 1 is the
  // only protocol we care about.
  //
  // For more detail, see
  // https://github.com/Grumbel/xboxdrv/blob/master/PROTOCOL
  io_iterator_t iter;
  kr = (*device_)->CreateInterfaceIterator(device_, &request, &iter);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;
  base::mac::ScopedIOObject<io_iterator_t> iter_ref(iter);

  // There should be exactly one USB interface which matches the requested
  // settings.
  io_service_t usb_interface = IOIteratorNext(iter);
  if (!usb_interface)
    return OPEN_FAILED;

  // We need to make an InterfaceInterface to communicate with the device
  // endpoint. This is the same process as earlier: first make a
  // PluginInterface from the io_service then make the InterfaceInterface from
  // that.
  IOCFPlugInInterface** plugin_interface;
  kr = IOCreatePlugInInterfaceForService(
      usb_interface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID,
      &plugin_interface, &score);
  if (kr != KERN_SUCCESS || !plugin_interface)
    return OPEN_FAILED;
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> interface_ref(
      plugin_interface);

  // Release the USB interface, and any subsequent interfaces returned by the
  // iterator. (There shouldn't be any, but in case a future device does
  // contain more interfaces, this will serve to avoid memory leaks.)
  do {
    IOObjectRelease(usb_interface);
  } while ((usb_interface = IOIteratorNext(iter)));

  // Actually create the interface.
  res = (*plugin_interface)
            ->QueryInterface(plugin_interface,
                             CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID300),
                             (LPVOID*)&interface_);

  if (!SUCCEEDED(res) || !interface_)
    return OPEN_FAILED;

  // Actually open the interface.
  kr = (*interface_)->USBInterfaceOpen(interface_);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;
  interface_is_open_ = true;

  CFRunLoopSourceRef source_ref;
  kr = (*interface_)->CreateInterfaceAsyncEventSource(interface_, &source_ref);
  if (kr != KERN_SUCCESS || !source_ref)
    return OPEN_FAILED;
  source_.reset(source_ref);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), source_, kCFRunLoopDefaultMode);

  // The interface should have two pipes. Pipe 1 with direction kUSBIn and pipe
  // 2 with direction kUSBOut. Both pipes should have type kUSBInterrupt.
  uint8_t num_endpoints;
  kr = (*interface_)->GetNumEndpoints(interface_, &num_endpoints);
  if (kr != KERN_SUCCESS || num_endpoints < 2)
    return OPEN_FAILED;

  for (int i = 1; i <= 2; i++) {
    uint8_t direction;
    uint8_t number;
    uint8_t transfer_type;
    uint16_t max_packet_size;
    uint8_t interval;

    kr = (*interface_)
             ->GetPipeProperties(interface_, i, &direction, &number,
                                 &transfer_type, &max_packet_size, &interval);
    if (kr != KERN_SUCCESS || transfer_type != kUSBInterrupt)
      return OPEN_FAILED;
    if (i == read_endpoint_) {
      if (direction != kUSBIn)
        return OPEN_FAILED;
      read_buffer_.reset(new uint8_t[max_packet_size]);
      read_buffer_size_ = max_packet_size;
      QueueRead();
    } else if (i == control_endpoint_) {
      if (direction != kUSBOut)
        return OPEN_FAILED;
      if (controller_type_ == XBOX_ONE_CONTROLLER_2013 ||
          controller_type_ == XBOX_ONE_CONTROLLER_2015 ||
          controller_type_ == XBOX_ONE_ELITE_CONTROLLER ||
          controller_type_ == XBOX_ONE_S_CONTROLLER ||
          controller_type_ == XBOX_ADAPTIVE_CONTROLLER)
        WriteXboxOneInit();
    }
  }

  // The location ID is unique per controller, and can be used to track
  // controllers through reconnections (though if a controller is detached from
  // one USB hub and attached to another, the location ID will change).
  kr = (*device_)->GetLocationID(device_, &location_id_);
  if (kr != KERN_SUCCESS)
    return OPEN_FAILED;

  return OPEN_SUCCEEDED;
}

void XboxControllerMac::SetLEDPattern(LEDPattern pattern) {
  led_pattern_ = pattern;
  const UInt8 length = 3;

  // This buffer will be released in WriteComplete when WritePipeAsync
  // finishes.
  UInt8* buffer = new UInt8[length];
  buffer[0] = static_cast<UInt8>(CONTROL_MESSAGE_SET_LED);
  buffer[1] = length;
  buffer[2] = static_cast<UInt8>(pattern);
  kern_return_t kr =
      (*interface_)
          ->WritePipeAsync(interface_, control_endpoint_, buffer,
                           (UInt32)length, WriteComplete, buffer);
  if (kr != KERN_SUCCESS) {
    delete[] buffer;
    IOError();
    return;
  }
}

uint16_t XboxControllerMac::GetVendorId() const {
  switch (controller_type_) {
    case XBOX_360_CONTROLLER:
    case XBOX_ONE_CONTROLLER_2013:
    case XBOX_ONE_CONTROLLER_2015:
    case XBOX_ONE_ELITE_CONTROLLER:
    case XBOX_ONE_S_CONTROLLER:
    case XBOX_ADAPTIVE_CONTROLLER:
      return kVendorMicrosoft;
    default:
      return 0;
  }
}

uint16_t XboxControllerMac::GetProductId() const {
  switch (controller_type_) {
    case XBOX_360_CONTROLLER:
      return kProductXbox360Controller;
    case XBOX_ONE_CONTROLLER_2013:
      return kProductXboxOneController2013;
    case XBOX_ONE_CONTROLLER_2015:
      return kProductXboxOneController2015;
    case XBOX_ONE_ELITE_CONTROLLER:
      return kProductXboxOneEliteController;
    case XBOX_ONE_S_CONTROLLER:
      return kProductXboxOneSController;
    case XBOX_ADAPTIVE_CONTROLLER:
      return kProductXboxAdaptiveController;
    default:
      return 0;
  }
}

XboxControllerMac::ControllerType XboxControllerMac::GetControllerType() const {
  return controller_type_;
}

std::string XboxControllerMac::GetControllerTypeString() const {
  switch (controller_type_) {
    case XBOX_360_CONTROLLER:
      return "Xbox 360 Controller";
    case XBOX_ONE_CONTROLLER_2013:
    case XBOX_ONE_CONTROLLER_2015:
    case XBOX_ONE_ELITE_CONTROLLER:
    case XBOX_ONE_S_CONTROLLER:
    case XBOX_ADAPTIVE_CONTROLLER:
      return "Xbox One Controller";
    default:
      return "Unrecognized Controller";
  }
}

std::string XboxControllerMac::GetIdString() const {
  return base::StringPrintf("%s (STANDARD GAMEPAD Vendor: %04x Product: %04x)",
                            GetControllerTypeString().c_str(), GetVendorId(),
                            GetProductId());
}

bool XboxControllerMac::SupportsVibration() const {
  // The Xbox Adaptive Controller has no vibration actuators.
  return controller_type_ != XBOX_ADAPTIVE_CONTROLLER;
}

// static
void XboxControllerMac::WriteComplete(void* context,
                                      IOReturn result,
                                      void* arg0) {
  UInt8* buffer = static_cast<UInt8*>(context);
  delete[] buffer;

  // Ignoring any errors sending data, because they will usually only occur
  // when the device is disconnected, in which case it really doesn't matter if
  // the data got to the controller or not.
  if (result != kIOReturnSuccess)
    return;
}

// static
void XboxControllerMac::GotData(void* context, IOReturn result, void* arg0) {
  size_t bytes_read = reinterpret_cast<size_t>(arg0);
  XboxControllerMac* controller = static_cast<XboxControllerMac*>(context);

  if (result != kIOReturnSuccess) {
    // This will happen if the device was disconnected. The gamepad has
    // probably been destroyed by a meteorite.
    controller->IOError();
    return;
  }

  if (controller->GetControllerType() == XBOX_360_CONTROLLER)
    controller->ProcessXbox360Packet(bytes_read);
  else
    controller->ProcessXboxOnePacket(bytes_read);

  // Queue up another read.
  controller->QueueRead();
}

void XboxControllerMac::ProcessXbox360Packet(size_t length) {
  if (length < kXbox360HeaderBytes)
    return;
  DCHECK(length <= read_buffer_size_);
  if (length > read_buffer_size_) {
    IOError();
    return;
  }
  uint8_t* buffer = read_buffer_.get();

  if (buffer[1] != length)
    // Length in packet doesn't match length reported by USB.
    return;

  uint8_t type = buffer[0];
  buffer += kXbox360HeaderBytes;
  length -= kXbox360HeaderBytes;
  switch (type) {
    case STATUS_MESSAGE_BUTTONS: {
      if (length != sizeof(Xbox360ButtonData))
        return;
      Xbox360ButtonData* data = reinterpret_cast<Xbox360ButtonData*>(buffer);
      Data normalized_data;
      NormalizeXbox360ButtonData(*data, &normalized_data);
      if (delegate_)
        delegate_->XboxControllerGotData(this, normalized_data);
      break;
    }
    case STATUS_MESSAGE_LED:
      if (length != 3)
        return;
      // The controller sends one of these messages every time the LED pattern
      // is set, as well as once when it is plugged in.
      if (led_pattern_ == LED_NUM_PATTERNS && buffer[0] < LED_NUM_PATTERNS)
        led_pattern_ = static_cast<LEDPattern>(buffer[0]);
      break;
    default:
      // Unknown packet: ignore!
      break;
  }
}

void XboxControllerMac::ProcessXboxOnePacket(size_t length) {
  if (length < kXboxOneHeaderBytes)
    return;
  DCHECK(length <= read_buffer_size_);
  if (length > read_buffer_size_) {
    IOError();
    return;
  }
  uint8_t* buffer = read_buffer_.get();
  uint8_t type = buffer[0];
  bool needs_ack = (buffer[1] == 0x30);
  uint8_t sequence_number = buffer[2];

  buffer += kXboxOneHeaderBytes;
  length -= kXboxOneHeaderBytes;
  switch (type) {
    case XBOX_ONE_STATUS_MESSAGE_BUTTONS: {
      if (length != sizeof(XboxOneButtonData) &&
          length != kXboxOneEliteButtonDataBytes &&
          length != kXboxAdaptiveButtonDataBytes) {
        return;
      }
      XboxOneButtonData* data = reinterpret_cast<XboxOneButtonData*>(buffer);
      Data normalized_data;
      NormalizeXboxOneButtonData(*data, &normalized_data);
      if (delegate_)
        delegate_->XboxControllerGotData(this, normalized_data);
      break;
    }
    case XBOX_ONE_STATUS_MESSAGE_GUIDE: {
      if (length != sizeof(XboxOneGuideData))
        return;
      XboxOneGuideData* data = reinterpret_cast<XboxOneGuideData*>(buffer);
      delegate_->XboxControllerGotGuideData(this, data->down);
      // The Xbox One S controller requires these reports to be acked.
      if (needs_ack)
        WriteXboxOneAckGuide(sequence_number);
      break;
    }
    default:
      // Unknown packet: ignore!
      break;
  }
}

void XboxControllerMac::QueueRead() {
  kern_return_t kr =
      (*interface_)
          ->ReadPipeAsync(interface_, read_endpoint_, read_buffer_.get(),
                          read_buffer_size_, GotData, this);
  if (kr != KERN_SUCCESS)
    IOError();
}

void XboxControllerMac::IOError() {
  // Ignore errors that occur while the controller is being shut down.
  if (delegate_ && !IsShuttingDown())
    delegate_->XboxControllerError(this);
}

void XboxControllerMac::WriteXbox360Rumble(uint8_t strong_magnitude,
                                           uint8_t weak_magnitude) {
  const UInt8 length = sizeof(Xbox360RumbleData);

  // This buffer will be released in WriteComplete when WritePipeAsync
  // finishes.
  UInt8* buffer = new UInt8[length];

  Xbox360RumbleData* rumble_data = reinterpret_cast<Xbox360RumbleData*>(buffer);
  memset(buffer, 0, length);
  rumble_data->command = 0x00;  // Rumble
  rumble_data->size = length;

  // Set rumble intensities.
  rumble_data->big = strong_magnitude;
  rumble_data->little = weak_magnitude;

  kern_return_t kr =
      (*interface_)
          ->WritePipeAsync(interface_, control_endpoint_, buffer,
                           (UInt32)length, WriteComplete, buffer);
  if (kr != KERN_SUCCESS) {
    delete[] buffer;
    IOError();
    return;
  }
}

void XboxControllerMac::WriteXboxOneInit() {
  const UInt8 length = 5;

  // This buffer will be released in WriteComplete when WritePipeAsync
  // finishes.
  UInt8* buffer = new UInt8[length];
  buffer[0] = 0x05;
  buffer[1] = 0x20;
  buffer[2] = 0x00;
  buffer[3] = 0x01;
  buffer[4] = 0x00;
  kern_return_t kr =
      (*interface_)
          ->WritePipeAsync(interface_, control_endpoint_, buffer,
                           (UInt32)length, WriteComplete, buffer);
  if (kr != KERN_SUCCESS) {
    delete[] buffer;
    IOError();
    return;
  }
}

void XboxControllerMac::WriteXboxOneRumble(uint8_t strong_magnitude,
                                           uint8_t weak_magnitude) {
  const UInt8 length = sizeof(XboxOneRumbleData);

  // This buffer will be released in WriteComplete when WritePipeAsync
  // finishes.
  UInt8* buffer = new UInt8[length];

  XboxOneRumbleData* rumble_data = reinterpret_cast<XboxOneRumbleData*>(buffer);
  rumble_data->command = 0x09;
  rumble_data->dummy1 = 0x00;
  rumble_data->counter = counter_++;
  rumble_data->size = 0x09;
  rumble_data->mode = 0x00;
  rumble_data->rumble_mask = 0x0f;
  rumble_data->duration = 0xff;
  rumble_data->period = 0x00;
  rumble_data->extra = 0x00;

  // Set rumble intensities.
  rumble_data->trigger_left = 0x00;
  rumble_data->trigger_right = 0x00;
  rumble_data->strong_magnitude = strong_magnitude;
  rumble_data->weak_magnitude = weak_magnitude;

  kern_return_t kr =
      (*interface_)
          ->WritePipeAsync(interface_, control_endpoint_, buffer,
                           (UInt32)length, WriteComplete, buffer);
  if (kr != KERN_SUCCESS) {
    delete[] buffer;
    IOError();
    return;
  }
}

// Ack the guide report. The contents of the ack report are modeled after
// xpad's xboxone_ack_mode_report. See the mode_report_ack buffer defined in
// this commit:
// https://github.com/torvalds/linux/commit/57b8443d3e5bd046a519ff714ca31c64c7f04309
void XboxControllerMac::WriteXboxOneAckGuide(uint8_t sequence_number) {
  const UInt8 length = 13;

  UInt8* buffer = new UInt8[length];
  buffer[0] = 0x01;
  buffer[1] = 0x20;
  buffer[2] = sequence_number;
  buffer[3] = 0x09;
  buffer[4] = 0x00;
  buffer[5] = 0x07;
  buffer[6] = 0x20;
  buffer[7] = 0x02;
  buffer[8] = 0x00;
  buffer[9] = 0x00;
  buffer[10] = 0x00;
  buffer[11] = 0x00;
  buffer[12] = 0x00;
  kern_return_t kr =
      (*interface_)
          ->WritePipeAsync(interface_, control_endpoint_, buffer,
                           (UInt32)length, WriteComplete, buffer);
  if (kr != KERN_SUCCESS) {
    delete[] buffer;
    IOError();
    return;
  }
}

base::WeakPtr<AbstractHapticGamepad> XboxControllerMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
