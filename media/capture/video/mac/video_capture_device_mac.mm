// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_mac.h"

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/timestamp_constants.h"
#include "media/capture/mojom/image_capture_types.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"
#include "ui/gfx/geometry/size.h"

using ScopedIOUSBInterfaceInterface =
    base::mac::ScopedIOPluginInterface<IOUSBInterfaceInterface220>;

@implementation DeviceNameAndTransportType

- (instancetype)initWithName:(NSString*)deviceName
               transportType:(int32_t)transportType {
  if (self = [super init]) {
    _deviceName.reset([deviceName copy]);
    _transportType = transportType;
  }
  return self;
}

- (NSString*)deviceName {
  return _deviceName;
}

- (int32_t)transportType {
  return _transportType;
}

@end  // @implementation DeviceNameAndTransportType

namespace media {

// Mac specific limits for minimum and maximum frame rate.
const float kMinFrameRate = 1.0f;
const float kMaxFrameRate = 60.0f;

// In device identifiers, the USB VID and PID are stored in 4 bytes each.
const size_t kVidPidSize = 4;

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
const int kCtZoomAbsoluteControl = 0x0b;     // CT_ZOOM_ABSOLUTE_CONTROL
const int kCtPanTiltAbsoluteControl = 0x0d;  // CT_PANTILT_ABSOLUTE_CONTROL
// Sec. A.9.5 "Processing Unit Control Selectors".
const int kPuPowerLineFrequencyControl =
    0x5;  // PU_POWER_LINE_FREQUENCY_CONTROL
// Sec. 4.2.2.3.5 "Power Line Frequency Control".
const int k50Hz = 1;
const int k60Hz = 2;
const int kPuPowerLineFrequencyControlCommandSize = 1;
// Sec. 4.2.2.1.11 "Zoom (Absolute) Control".
const int kCtZoomAbsoluteControlCommandSize = 2;
// Sec. 4.2.2.1.13 "PanTilt (Absolute) Control".
const int kCtPanTiltAbsoluteControlCommandSize = 8;

// Addition to the IOUSB family of structures, with subtype and unit ID.
// Sec. 3.7.2 "Class-Specific VC Interface Descriptor"
typedef struct VcCsInterfaceDescriptor {
  IOUSBDescriptorHeader header;
  UInt8 bDescriptorSubType;
  UInt8 bUnitID;
} VcCsInterfaceDescriptor;

static bool FindDeviceWithVendorAndProductIds(int vendor_id,
                                              int product_id,
                                              io_iterator_t* usb_iterator) {
  // Compose a search dictionary with vendor and product ID.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query_dictionary(
      IOServiceMatching(kIOUSBDeviceClassName));
  CFDictionarySetValue(query_dictionary, CFSTR(kUSBVendorName),
                       base::mac::NSToCFCast(@(vendor_id)));
  CFDictionarySetValue(query_dictionary, CFSTR(kUSBProductName),
                       base::mac::NSToCFCast(@(product_id)));

  kern_return_t kr = IOServiceGetMatchingServices(
      kIOMasterPortDefault, query_dictionary.release(), usb_iterator);
  if (kr != kIOReturnSuccess) {
    VLOG(1) << "No devices found with specified Vendor and Product ID.";
    return false;
  }
  return true;
}

// Tries to create a user-side device interface for a given USB device. Returns
// true if interface was found and passes it back in |device_interface|. The
// caller should release |device_interface|.
static bool FindDeviceInterfaceInUsbDevice(
    const int vendor_id,
    const int product_id,
    const io_service_t usb_device,
    IOUSBDeviceInterface*** device_interface) {
  // Create a plugin, i.e. a user-side controller to manipulate USB device.
  IOCFPlugInInterface** plugin;
  SInt32 score;  // Unused, but required for IOCreatePlugInInterfaceForService.
  kern_return_t kr = IOCreatePlugInInterfaceForService(
      usb_device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin,
      &score);
  if (kr != kIOReturnSuccess || !plugin) {
    VLOG(1) << "IOCreatePlugInInterfaceForService";
    return false;
  }
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_ref(plugin);

  // Fetch the Device Interface from the plugin.
  HRESULT res = (*plugin)->QueryInterface(
      plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
      reinterpret_cast<LPVOID*>(device_interface));
  if (!SUCCEEDED(res) || !*device_interface) {
    VLOG(1) << "QueryInterface, couldn't create interface to USB";
    return false;
  }
  return true;
}

// Tries to find a Video Control type interface inside a general USB device
// interface |device_interface|, and returns it in |video_control_interface| if
// found. The returned interface must be released in the caller.
static bool FindVideoControlInterfaceInDeviceInterface(
    IOUSBDeviceInterface** device_interface,
    IOCFPlugInInterface*** video_control_interface) {
  // Create an iterator to the list of Video-AVControl interfaces of the device,
  // then get the first interface in the list.
  io_iterator_t interface_iterator;
  IOUSBFindInterfaceRequest interface_request = {
      .bInterfaceClass = kUSBVideoInterfaceClass,
      .bInterfaceSubClass = kUSBVideoControlSubClass,
      .bInterfaceProtocol = kIOUSBFindInterfaceDontCare,
      .bAlternateSetting = kIOUSBFindInterfaceDontCare};
  kern_return_t kr =
      (*device_interface)
          ->CreateInterfaceIterator(device_interface, &interface_request,
                                    &interface_iterator);
  if (kr != kIOReturnSuccess) {
    VLOG(1) << "Could not create an iterator to the device's interfaces.";
    return false;
  }
  base::mac::ScopedIOObject<io_iterator_t> iterator_ref(interface_iterator);

  // There should be just one interface matching the class-subclass desired.
  io_service_t found_interface;
  found_interface = IOIteratorNext(interface_iterator);
  if (!found_interface) {
    VLOG(1) << "Could not find a Video-AVControl interface in the device.";
    return false;
  }
  base::mac::ScopedIOObject<io_service_t> found_interface_ref(found_interface);

  // Create a user side controller (i.e. a "plugin") for the found interface.
  SInt32 score;
  kr = IOCreatePlugInInterfaceForService(
      found_interface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID,
      video_control_interface, &score);
  if (kr != kIOReturnSuccess || !*video_control_interface) {
    VLOG(1) << "IOCreatePlugInInterfaceForService";
    return false;
  }
  return true;
}

// Creates a control interface for |plugin_interface| and produces a command to
// set the appropriate Power Line frequency for flicker removal.
static void SetAntiFlickerInVideoControlInterface(
    IOCFPlugInInterface** plugin_interface,
    const PowerLineFrequency frequency) {
  // Create, the control interface for the found plugin, and release
  // the intermediate plugin.
  ScopedIOUSBInterfaceInterface control_interface;
  HRESULT res =
      (*plugin_interface)
          ->QueryInterface(
              plugin_interface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
              reinterpret_cast<LPVOID*>(control_interface.InitializeInto()));
  if (!SUCCEEDED(res) || !control_interface) {
    VLOG(1) << "Couldn’t create control interface";
    return;
  }

  // Find the device's unit ID presenting type 0x24 (kVcCsInterface) and
  // subtype 0x5 (kVcProcessingUnit). Inside this unit is where we find the
  // power line frequency removal setting, and this id is device dependent.
  int real_unit_id = -1;
  IOUSBDescriptorHeader* descriptor = NULL;
  while ((descriptor =
              (*control_interface)
                  ->FindNextAssociatedDescriptor(control_interface.get(),
                                                 descriptor, kVcCsInterface))) {
    auto* cs_descriptor =
        reinterpret_cast<VcCsInterfaceDescriptor*>(descriptor);
    if (cs_descriptor->bDescriptorSubType == kVcProcessingUnit) {
      real_unit_id = cs_descriptor->bUnitID;
      break;
    }
  }
  VLOG_IF(1, real_unit_id == -1)
      << "This USB device doesn't seem to have a "
      << " VC_PROCESSING_UNIT, anti-flicker not available";
  if (real_unit_id == -1)
    return;

  if ((*control_interface)->USBInterfaceOpen(control_interface) !=
      kIOReturnSuccess) {
    VLOG(1) << "Unable to open control interface";
    return;
  }

  // Create the control request and launch it to the device's control interface.
  // Note how the wIndex needs the interface number OR'ed in the lowest bits.
  IOUSBDevRequest command;
  command.bmRequestType = USBmakebmRequestType(UInt8{kUSBOut}, UInt8{kUSBClass},
                                               UInt8{kUSBInterface});
  command.bRequest = kVcRequestCodeSetCur;
  UInt8 interface_number;
  (*control_interface)
      ->GetInterfaceNumber(control_interface, &interface_number);
  command.wIndex = (real_unit_id << 8) | interface_number;
  const int selector = kPuPowerLineFrequencyControl;
  command.wValue = (selector << 8);
  command.wLength = kPuPowerLineFrequencyControlCommandSize;
  command.wLenDone = 0;
  int power_line_flag_value =
      (frequency == PowerLineFrequency::FREQUENCY_50HZ) ? k50Hz : k60Hz;
  command.pData = &power_line_flag_value;

  IOReturn ret =
      (*control_interface)->ControlRequest(control_interface, 0, &command);
  VLOG_IF(1, ret != kIOReturnSuccess) << "Anti-flicker control request"
                                          << " failed (0x" << std::hex << ret
                                          << "), unit id: " << real_unit_id;
  VLOG_IF(1, ret == kIOReturnSuccess) << "Anti-flicker set to "
                                       << static_cast<int>(frequency) << "Hz";

  (*control_interface)->USBInterfaceClose(control_interface);
}

// Sets the flicker removal in a USB webcam identified by |vendor_id| and
// |product_id|, if available. The process includes first finding all USB
// devices matching the specified |vendor_id| and |product_id|; for each
// matching device, a device interface, and inside it a video control interface
// are created. The latter is used to a send a power frequency setting command.
static void SetAntiFlickerInUsbDevice(const int vendor_id,
                                      const int product_id,
                                      const PowerLineFrequency frequency) {
  if (frequency == PowerLineFrequency::FREQUENCY_DEFAULT)
    return;
  DVLOG(1) << "Setting Power Line Frequency to " << static_cast<int>(frequency)
           << " Hz, device " << std::hex << vendor_id << "-" << product_id;

  base::mac::ScopedIOObject<io_iterator_t> usb_iterator;
  if (!FindDeviceWithVendorAndProductIds(vendor_id, product_id,
                                         usb_iterator.InitializeInto())) {
    return;
  }

  while (io_service_t usb_device = IOIteratorNext(usb_iterator)) {
    base::mac::ScopedIOObject<io_service_t> usb_device_ref(usb_device);

    IOUSBDeviceInterface** device_interface = NULL;
    if (!FindDeviceInterfaceInUsbDevice(vendor_id, product_id, usb_device,
                                        &device_interface)) {
      return;
    }
    base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface>
        device_interface_ref(device_interface);

    IOCFPlugInInterface** video_control_interface = NULL;
    if (!FindVideoControlInterfaceInDeviceInterface(device_interface,
                                                    &video_control_interface)) {
      return;
    }
    base::mac::ScopedIOPluginInterface<IOCFPlugInInterface>
        plugin_interface_ref(video_control_interface);

    SetAntiFlickerInVideoControlInterface(video_control_interface, frequency);
  }
}

// Create an empty IOUSBDevRequest for a USB device to either control or get
// information from pan, tilt, and zoom controls.
static IOUSBDevRequest CreateEmptyPanTiltZoomRequest(
    IOUSBInterfaceInterface220** control_interface,
    int unit_id,
    int request_code,
    int endpoint_direction,
    int control_selector,
    int control_command_size) {
  DCHECK((endpoint_direction == kUSBIn) || (endpoint_direction == kUSBOut));
  UInt8 interface_number;
  (*control_interface)
      ->GetInterfaceNumber(control_interface, &interface_number);
  IOUSBDevRequest command;
  command.bmRequestType = USBmakebmRequestType(
      endpoint_direction, UInt8{kUSBClass}, UInt8{kUSBInterface});
  command.bRequest = request_code;
  command.wIndex = (unit_id << 8) | interface_number;
  command.wValue = (control_selector << 8);
  command.wLength = control_command_size;
  command.wLenDone = 0;
  return command;
}

// Send USB request to get information about pan and tilt controls.
// Returns true if the request is successful. To send the request, the interface
// must be opened already.
static bool SendPanTiltControlRequest(
    IOUSBInterfaceInterface220** control_interface,
    int unit_id,
    int request_code,
    int* pan_result,
    int* tilt_result) {
  IOUSBDevRequest command = CreateEmptyPanTiltZoomRequest(
      control_interface, unit_id, request_code, kUSBIn,
      kCtPanTiltAbsoluteControl, kCtPanTiltAbsoluteControlCommandSize);
  int32_t data[2];
  command.pData = &data;

  IOReturn ret =
      (*control_interface)->ControlRequest(control_interface, 0, &command);
  VLOG_IF(1, ret != kIOReturnSuccess)
      << "Control pan tilt request"
      << " failed (0x" << std::hex << ret << "), unit id: " << unit_id;
  if (ret != kIOReturnSuccess)
    return false;

  *pan_result = USBToHostLong(data[0]);
  *tilt_result = USBToHostLong(data[1]);
  return true;
}

// Send USB request to get information about zoom control.
// Returns true if the request is successful. To send the request, the interface
// must be opened already.
static bool SendZoomControlRequest(
    IOUSBInterfaceInterface220** control_interface,
    int unit_id,
    int request_code,
    int* result) {
  IOUSBDevRequest command = CreateEmptyPanTiltZoomRequest(
      control_interface, unit_id, request_code, kUSBIn, kCtZoomAbsoluteControl,
      kCtZoomAbsoluteControlCommandSize);
  uint16_t data;
  command.pData = &data;

  IOReturn ret =
      (*control_interface)->ControlRequest(control_interface, 0, &command);
  VLOG_IF(1, ret != kIOReturnSuccess)
      << "Control zoom request"
      << " failed (0x" << std::hex << ret << "), unit id: " << unit_id;
  if (ret != kIOReturnSuccess)
    return false;

  *result = USBToHostLong(data);
  return true;
}

// Retrieves the control range and current value of pan and tilt controls.
// The interface must be opened already.
static void GetPanTiltControlRangeAndCurrent(
    IOUSBInterfaceInterface220** control_interface,
    int unit_id,
    mojom::Range* pan_range,
    mojom::Range* tilt_range) {
  int pan_max, pan_min, pan_step, pan_current;
  int tilt_max, tilt_min, tilt_step, tilt_current;
  if (!SendPanTiltControlRequest(control_interface, unit_id,
                                 kVcRequestCodeGetMax, &pan_max, &tilt_max) ||
      !SendPanTiltControlRequest(control_interface, unit_id,
                                 kVcRequestCodeGetMin, &pan_min, &tilt_min) ||
      !SendPanTiltControlRequest(control_interface, unit_id,
                                 kVcRequestCodeGetRes, &pan_step, &tilt_step) ||
      !SendPanTiltControlRequest(control_interface, unit_id,
                                 kVcRequestCodeGetCur, &pan_current,
                                 &tilt_current)) {
    return;
  }
  pan_range->max = pan_max;
  pan_range->min = pan_min;
  pan_range->step = pan_step;
  pan_range->current = pan_current;
  tilt_range->max = tilt_max;
  tilt_range->min = tilt_min;
  tilt_range->step = tilt_step;
  tilt_range->current = tilt_current;
}

// Retrieves the control range and current value of a zoom control. The
// interface must be opened already.
static void GetZoomControlRangeAndCurrent(
    IOUSBInterfaceInterface220** control_interface,
    int unit_id,
    mojom::Range* zoom_range) {
  int max, min, step, current;
  if (!SendZoomControlRequest(control_interface, unit_id, kVcRequestCodeGetMax,
                              &max) ||
      !SendZoomControlRequest(control_interface, unit_id, kVcRequestCodeGetMin,
                              &min) ||
      !SendZoomControlRequest(control_interface, unit_id, kVcRequestCodeGetRes,
                              &step) ||
      !SendZoomControlRequest(control_interface, unit_id, kVcRequestCodeGetCur,
                              &current)) {
    return;
  }
  zoom_range->max = max;
  zoom_range->min = min;
  zoom_range->step = step;
  zoom_range->current = current;
}

// Set pan and tilt values for a USB camera device. The interface must be opened
// already.
static void SetPanTiltInUsbDevice(
    IOUSBInterfaceInterface220** control_interface,
    int unit_id,
    absl::optional<int> pan,
    absl::optional<int> tilt) {
  if (!pan.has_value() && !tilt.has_value())
    return;

  int pan_current, tilt_current;
  if ((!pan.has_value() || !tilt.has_value()) &&
      !SendPanTiltControlRequest(control_interface, unit_id,
                                 kVcRequestCodeGetCur, &pan_current,
                                 &tilt_current)) {
    return;
  }

  uint32_t pan_tilt_data[2];
  pan_tilt_data[0] =
      CFSwapInt32HostToLittle((uint32_t)pan.value_or(pan_current));
  pan_tilt_data[1] =
      CFSwapInt32HostToLittle((uint32_t)tilt.value_or(tilt_current));

  IOUSBDevRequest command = CreateEmptyPanTiltZoomRequest(
      control_interface, unit_id, kVcRequestCodeSetCur, kUSBOut,
      kCtPanTiltAbsoluteControl, kCtPanTiltAbsoluteControlCommandSize);
  command.pData = pan_tilt_data;

  IOReturn ret =
      (*control_interface)->ControlRequest(control_interface, 0, &command);
  VLOG_IF(1, ret != kIOReturnSuccess)
      << "Control request"
      << " failed (0x" << std::hex << ret << "), unit id: " << unit_id
      << " pan value: " << pan.value_or(pan_current)
      << " tilt value: " << tilt.value_or(tilt_current);
  VLOG_IF(1, ret == kIOReturnSuccess)
      << "Setting pan value to " << pan.value_or(pan_current)
      << " and tilt value to " << tilt.value_or(tilt_current);
}

// Set zoom value for a USB camera device. The interface must be opened already.
static void SetZoomInUsbDevice(IOUSBInterfaceInterface220** control_interface,
                               int unit_id,
                               int zoom) {
  IOUSBDevRequest command = CreateEmptyPanTiltZoomRequest(
      control_interface, unit_id, kVcRequestCodeSetCur, kUSBOut,
      kCtZoomAbsoluteControl, kCtZoomAbsoluteControlCommandSize);
  command.pData = &zoom;

  IOReturn ret =
      (*control_interface)->ControlRequest(control_interface, 0, &command);
  VLOG_IF(1, ret != kIOReturnSuccess)
      << "Control request"
      << " failed (0x" << std::hex << ret << "), unit id: " << unit_id
      << " zoom value: " << zoom;
  VLOG_IF(1, ret == kIOReturnSuccess) << "Setting zoom value to " << zoom;
}

// Open the pan, tilt, zoom interface in a USB webcam identified by
// |device_model|. Returns interface when it is succcessfully opened. You have
// to close the interface manually when you're done.
static ScopedIOUSBInterfaceInterface OpenPanTiltZoomControlInterface(
    std::string device_model,
    int* unit_id) {
  if (device_model.length() <= 2 * kVidPidSize) {
    return ScopedIOUSBInterfaceInterface();
  }
  std::string vendor_id = device_model.substr(0, kVidPidSize);
  std::string product_id = device_model.substr(kVidPidSize + 1);
  int vendor_id_as_int, product_id_as_int;
  if (!base::HexStringToInt(vendor_id, &vendor_id_as_int) ||
      !base::HexStringToInt(product_id, &product_id_as_int)) {
    return ScopedIOUSBInterfaceInterface();
  }

  base::mac::ScopedIOObject<io_iterator_t> usb_iterator;
  if (!FindDeviceWithVendorAndProductIds(vendor_id_as_int, product_id_as_int,
                                         usb_iterator.InitializeInto())) {
    return ScopedIOUSBInterfaceInterface();
  }

  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface>
      video_control_interface;

  while (io_service_t usb_device = IOIteratorNext(usb_iterator)) {
    base::mac::ScopedIOObject<io_service_t> usb_device_ref(usb_device);
    base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface> device_interface;

    if (!FindDeviceInterfaceInUsbDevice(vendor_id_as_int, product_id_as_int,
                                        usb_device,
                                        device_interface.InitializeInto())) {
      continue;
    }

    if (FindVideoControlInterfaceInDeviceInterface(
            device_interface, video_control_interface.InitializeInto())) {
      break;
    }
  }

  if (video_control_interface == nullptr) {
    return ScopedIOUSBInterfaceInterface();
  }

  // Create the control interface for the found plugin, and release
  // the intermediate plugin.
  ScopedIOUSBInterfaceInterface control_interface;
  HRESULT res =
      (*video_control_interface)
          ->QueryInterface(
              video_control_interface,
              CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID220),
              reinterpret_cast<LPVOID*>(control_interface.InitializeInto()));
  if (!SUCCEEDED(res) || !control_interface) {
    VLOG(1) << "Couldn’t get control interface";
    return ScopedIOUSBInterfaceInterface();
  }

  // Find the device's unit ID presenting type 0x24 (kVcCsInterface) and
  // subtype 0x2 (kVcInputTerminal). Inside this unit is where we find the
  // settings for pan, tilt, and zoom, and this id is device dependent.
  IOUSBDescriptorHeader* descriptor = nullptr;
  while ((descriptor =
              (*control_interface)
                  ->FindNextAssociatedDescriptor(control_interface.get(),
                                                 descriptor, kVcCsInterface))) {
    auto* cs_descriptor =
        reinterpret_cast<VcCsInterfaceDescriptor*>(descriptor);
    if (cs_descriptor->bDescriptorSubType == kVcInputTerminal) {
      *unit_id = cs_descriptor->bUnitID;
      break;
    }
  }

  VLOG_IF(1, *unit_id == -1)
      << "This USB device doesn't seem to have a "
      << " VC_INPUT_TERMINAL. Pan, tilt, zoom are not available.";
  if (*unit_id == -1)
    return ScopedIOUSBInterfaceInterface();

  if ((*control_interface)->USBInterfaceOpen(control_interface) !=
      kIOReturnSuccess) {
    VLOG(1) << "Unable to open control interface";
    return ScopedIOUSBInterfaceInterface();
  }

  return control_interface;
}

VideoCaptureDeviceMac::VideoCaptureDeviceMac(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : device_descriptor_(device_descriptor),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      state_(kNotInitialized),
      capture_device_(nil),
      weak_factory_(this) {}

VideoCaptureDeviceMac::~VideoCaptureDeviceMac() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void VideoCaptureDeviceMac::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMac::AllocateAndStart");
  if (state_ != kIdle) {
    return;
  }

  client_ = std::move(client);
  if (device_descriptor_.capture_api == VideoCaptureApi::MACOSX_AVFOUNDATION) {
    LogMessage("Using AVFoundation for device: " +
               device_descriptor_.display_name());
  }

  NSString* deviceId = base::SysUTF8ToNSString(device_descriptor_.device_id);

  [capture_device_ setFrameReceiver:this];

  NSString* errorMessage = nil;
  if (![capture_device_ setCaptureDevice:deviceId errorMessage:&errorMessage]) {
    SetErrorState(VideoCaptureError::kMacSetCaptureDeviceFailed, FROM_HERE,
                  base::SysNSStringToUTF8(errorMessage));
    return;
  }

  capture_format_.frame_size = params.requested_format.frame_size;
  capture_format_.frame_rate =
      std::max(kMinFrameRate,
               std::min(params.requested_format.frame_rate, kMaxFrameRate));
  // Leave the pixel format selection to AVFoundation. The pixel format
  // will be passed to |ReceiveFrame|.
  capture_format_.pixel_format = PIXEL_FORMAT_UNKNOWN;

  if (!UpdateCaptureResolution())
    return;

  // Try setting the power line frequency removal (anti-flicker). The built-in
  // cameras are normally suspended so the configuration must happen right
  // before starting capture and during configuration.
  const std::string device_model = GetDeviceModelId(
      device_descriptor_.device_id, device_descriptor_.capture_api,
      device_descriptor_.transport_type);
  if (device_model.length() > 2 * kVidPidSize) {
    std::string vendor_id = device_model.substr(0, kVidPidSize);
    std::string product_id = device_model.substr(kVidPidSize + 1);
    int vendor_id_as_int, product_id_as_int;
    if (base::HexStringToInt(vendor_id, &vendor_id_as_int) &&
        base::HexStringToInt(product_id, &product_id_as_int)) {
      SetAntiFlickerInUsbDevice(vendor_id_as_int, product_id_as_int,
                                GetPowerLineFrequency(params));
    }
  }
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                 "startCapture");
    if (![capture_device_ startCapture]) {
      SetErrorState(VideoCaptureError::kMacCouldNotStartCaptureDevice,
                    FROM_HERE, "Could not start capture device.");
      return;
    }
  }

  client_->OnStarted();
  state_ = kCapturing;
}

void VideoCaptureDeviceMac::StopAndDeAllocate() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing || state_ == kError) << state_;

  NSString* errorMessage = nil;
  if (![capture_device_ setCaptureDevice:nil errorMessage:&errorMessage])
    LogMessage(base::SysNSStringToUTF8(errorMessage));

  [capture_device_ setFrameReceiver:nil];
  client_.reset();
  state_ = kIdle;
}

void VideoCaptureDeviceMac::TakePhoto(TakePhotoCallback callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCapturing) << state_;

  if (photo_callback_)  // Only one picture can be in flight at a time.
    return;

  photo_callback_ = std::move(callback);
  [capture_device_ takePhoto];
}

void VideoCaptureDeviceMac::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto photo_state = mojo::CreateEmptyPhotoState();

  photo_state->height = mojom::Range::New(
      capture_format_.frame_size.height(), capture_format_.frame_size.height(),
      capture_format_.frame_size.height(), 0 /* step */);
  photo_state->width = mojom::Range::New(
      capture_format_.frame_size.width(), capture_format_.frame_size.width(),
      capture_format_.frame_size.width(), 0 /* step */);

  const std::string device_model = GetDeviceModelId(
      device_descriptor_.device_id, device_descriptor_.capture_api,
      device_descriptor_.transport_type);
  int unit_id = -1;
  ScopedIOUSBInterfaceInterface control_interface(
      OpenPanTiltZoomControlInterface(device_model, &unit_id));
  if (control_interface) {
    GetPanTiltControlRangeAndCurrent(control_interface, unit_id,
                                     photo_state->pan.get(),
                                     photo_state->tilt.get());
    GetZoomControlRangeAndCurrent(control_interface, unit_id,
                                  photo_state->zoom.get());
    (*control_interface)->USBInterfaceClose(control_interface);
  }

  if ([capture_device_ isPortraitEffectSupported]) {
    bool isPortraitEffectActive = [capture_device_ isPortraitEffectActive];
    photo_state->supported_background_blur_modes = {
        isPortraitEffectActive ? mojom::BackgroundBlurMode::BLUR
                               : mojom::BackgroundBlurMode::OFF};
    photo_state->background_blur_mode = isPortraitEffectActive
                                            ? mojom::BackgroundBlurMode::BLUR
                                            : mojom::BackgroundBlurMode::OFF;
  }

  std::move(callback).Run(std::move(photo_state));
}

void VideoCaptureDeviceMac::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                            SetPhotoOptionsCallback callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Drop |callback| and return if there are any unsupported |settings|.
  // TODO(mcasas): centralise checks elsewhere, https://crbug.com/724285.
  if ((settings->has_width &&
       settings->width != capture_format_.frame_size.width()) ||
      (settings->has_height &&
       settings->height != capture_format_.frame_size.height()) ||
      settings->has_fill_light_mode || settings->has_red_eye_reduction) {
    return;
  }

  // Abort if background blur does not have already the desired value.
  if (settings->has_background_blur_mode &&
      (![capture_device_ isPortraitEffectSupported] ||
       settings->background_blur_mode !=
           ([capture_device_ isPortraitEffectActive]
                ? mojom::BackgroundBlurMode::BLUR
                : mojom::BackgroundBlurMode::OFF))) {
    return;
  }

  if (settings->has_pan || settings->has_tilt || settings->has_zoom) {
    const std::string device_model = GetDeviceModelId(
        device_descriptor_.device_id, device_descriptor_.capture_api,
        device_descriptor_.transport_type);
    int unit_id = -1;
    ScopedIOUSBInterfaceInterface control_interface(
        OpenPanTiltZoomControlInterface(device_model, &unit_id));
    if (!control_interface)
      return;

    if (settings->has_pan || settings->has_tilt) {
      SetPanTiltInUsbDevice(
          control_interface, unit_id,
          settings->has_pan ? absl::make_optional(settings->pan)
                            : absl::nullopt,
          settings->has_tilt ? absl::make_optional(settings->tilt)
                             : absl::nullopt);
    }
    if (settings->has_zoom) {
      SetZoomInUsbDevice(control_interface, unit_id, settings->zoom);
    }
    (*control_interface)->USBInterfaceClose(control_interface);
  }

  std::move(callback).Run(true);
}

void VideoCaptureDeviceMac::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!capture_device_)
    return;
  [capture_device_ setScaledResolutions:std::move(feedback.mapped_sizes)];
}

bool VideoCaptureDeviceMac::Init(VideoCaptureApi capture_api_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kNotInitialized);

  if (capture_api_type != VideoCaptureApi::MACOSX_AVFOUNDATION)
    return false;

  capture_device_.reset(
      [[VideoCaptureDeviceAVFoundation alloc] initWithFrameReceiver:this]);

  if (!capture_device_)
    return false;

  state_ = kIdle;
  return true;
}

void VideoCaptureDeviceMac::ReceiveFrame(const uint8_t* video_frame,
                                         int video_frame_length,
                                         const VideoCaptureFormat& frame_format,
                                         const gfx::ColorSpace color_space,
                                         int aspect_numerator,
                                         int aspect_denominator,
                                         base::TimeDelta timestamp) {
  if (capture_format_.frame_size != frame_format.frame_size) {
    ReceiveError(VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution,
                 FROM_HERE,
                 "Captured resolution " + frame_format.frame_size.ToString() +
                     ", and expected " + capture_format_.frame_size.ToString());
    return;
  }

  client_->OnIncomingCapturedData(video_frame, video_frame_length, frame_format,
                                  color_space, 0 /* clockwise_rotation */,
                                  false /* flip_y */, base::TimeTicks::Now(),
                                  timestamp);
}

void VideoCaptureDeviceMac::ReceiveExternalGpuMemoryBufferFrame(
    CapturedExternalVideoBuffer frame,
    std::vector<CapturedExternalVideoBuffer> scaled_frames,
    base::TimeDelta timestamp) {
  if (capture_format_.frame_size != frame.format.frame_size) {
    ReceiveError(VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution,
                 FROM_HERE,
                 "Captured resolution " + frame.format.frame_size.ToString() +
                     ", and expected " + capture_format_.frame_size.ToString());
    return;
  }
  client_->OnIncomingCapturedExternalBuffer(
      std::move(frame), std::move(scaled_frames), base::TimeTicks::Now(),
      timestamp, gfx::Rect(capture_format_.frame_size));
}

void VideoCaptureDeviceMac::OnPhotoTaken(const uint8_t* image_data,
                                         size_t image_length,
                                         const std::string& mime_type) {
  DCHECK(photo_callback_);
  if (!image_data || !image_length) {
    OnPhotoError();
    return;
  }

  mojom::BlobPtr blob = mojom::Blob::New();
  blob->data.assign(image_data, image_data + image_length);
  blob->mime_type = mime_type;
  std::move(photo_callback_).Run(std::move(blob));
}

void VideoCaptureDeviceMac::OnPhotoError() {
  VLOG(1) << __func__ << " error taking picture";
  photo_callback_.Reset();
}

void VideoCaptureDeviceMac::ReceiveError(VideoCaptureError error,
                                         const base::Location& from_here,
                                         const std::string& reason) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceMac::SetErrorState,
                     weak_factory_.GetWeakPtr(), error, from_here, reason));
}

void VideoCaptureDeviceMac::LogMessage(const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->OnLog(message);
}

void VideoCaptureDeviceMac::ReceiveCaptureConfigurationChanged() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceMac::OnCaptureConfigurationChanged,
                     weak_factory_.GetWeakPtr()));
}

void VideoCaptureDeviceMac::OnCaptureConfigurationChanged() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (client_) {
    client_->OnCaptureConfigurationChanged();
  }
}

void VideoCaptureDeviceMac::SetIsPortraitEffectSupportedForTesting(
    bool isPortraitEffectSupported) {
  [capture_device_
      setIsPortraitEffectSupportedForTesting:isPortraitEffectSupported];
}

void VideoCaptureDeviceMac::SetIsPortraitEffectActiveForTesting(
    bool isPortraitEffectActive) {
  [capture_device_ setIsPortraitEffectActiveForTesting:isPortraitEffectActive];
}

// static
std::string VideoCaptureDeviceMac::GetDeviceModelId(
    const std::string& device_id,
    VideoCaptureApi capture_api,
    VideoCaptureTransportType transport_type) {
  // Skip the AVFoundation's not USB nor built-in devices.
  if (capture_api == VideoCaptureApi::MACOSX_AVFOUNDATION &&
      transport_type != VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN)
    return "";
  if (capture_api == VideoCaptureApi::MACOSX_DECKLINK)
    return "";
  // Both PID and VID are 4 characters.
  if (device_id.size() < 2 * kVidPidSize)
    return "";

  // The last characters of device id is a concatenation of VID and then PID.
  const size_t vid_location = device_id.size() - 2 * kVidPidSize;
  std::string id_vendor = device_id.substr(vid_location, kVidPidSize);
  const size_t pid_location = device_id.size() - kVidPidSize;
  std::string id_product = device_id.substr(pid_location, kVidPidSize);

  return id_vendor + ":" + id_product;
}

// Check if the video capture device supports pan, tilt and zoom controls.
// static
VideoCaptureControlSupport VideoCaptureDeviceMac::GetControlSupport(
    const std::string& device_model) {
  VideoCaptureControlSupport control_support;

  int unit_id = -1;
  ScopedIOUSBInterfaceInterface control_interface(
      OpenPanTiltZoomControlInterface(device_model, &unit_id));
  if (!control_interface)
    return control_support;

  int zoom_max, zoom_min = 0;
  if (SendZoomControlRequest(control_interface, unit_id, kVcRequestCodeGetMax,
                             &zoom_max) &&
      SendZoomControlRequest(control_interface, unit_id, kVcRequestCodeGetMin,
                             &zoom_min) &&
      zoom_min < zoom_max) {
    control_support.zoom = true;
  }
  int pan_max, pan_min = 0;
  int tilt_max, tilt_min = 0;
  if (SendPanTiltControlRequest(control_interface, unit_id,
                                kVcRequestCodeGetMax, &pan_max, &tilt_max) &&
      SendPanTiltControlRequest(control_interface, unit_id,
                                kVcRequestCodeGetMin, &pan_min, &tilt_min)) {
    if (pan_min < pan_max)
      control_support.pan = true;
    if (tilt_min < tilt_max)
      control_support.tilt = true;
  }

  (*control_interface)->USBInterfaceClose(control_interface);
  return control_support;
}

void VideoCaptureDeviceMac::SetErrorState(VideoCaptureError error,
                                          const base::Location& from_here,
                                          const std::string& reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kError;
  client_->OnError(error, from_here, reason);
}

bool VideoCaptureDeviceMac::UpdateCaptureResolution() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMac::UpdateCaptureResolution");
  if (![capture_device_ setCaptureHeight:capture_format_.frame_size.height()
                                   width:capture_format_.frame_size.width()
                               frameRate:capture_format_.frame_rate]) {
    ReceiveError(VideoCaptureError::kMacUpdateCaptureResolutionFailed,
                 FROM_HERE, "Could not configure capture device.");
    return false;
  }
  return true;
}

}  // namespace media
