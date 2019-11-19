// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/base/timestamp_constants.h"
#include "media/capture/mojom/image_capture_types.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "ui/gfx/geometry/size.h"

@implementation DeviceNameAndTransportType

- (id)initWithName:(NSString*)deviceName transportType:(int32_t)transportType {
  if (self = [super init]) {
    deviceName_.reset([deviceName copy]);
    transportType_ = transportType;
  }
  return self;
}

- (NSString*)deviceName {
  return deviceName_;
}

- (int32_t)transportType {
  return transportType_;
}

@end  // @implementation DeviceNameAndTransportType

namespace media {

// Mac specific limits for minimum and maximum frame rate.
const float kMinFrameRate = 1.0f;
const float kMaxFrameRate = 30.0f;

// In device identifiers, the USB VID and PID are stored in 4 bytes each.
const size_t kVidPidSize = 4;

// The following constants are extracted from the specification "Universal
// Serial Bus Device Class Definition for Video Devices", Rev. 1.1 June 1, 2005.
// http://www.usb.org/developers/devclass_docs/USB_Video_Class_1_1.zip
// CS_INTERFACE: Sec. A.4 "Video Class-Specific Descriptor Types".
const int kVcCsInterface = 0x24;
// VC_PROCESSING_UNIT: Sec. A.5 "Video Class-Specific VC Interface Descriptor
// Subtypes".
const int kVcProcessingUnit = 0x5;
// SET_CUR: Sec. A.8 "Video Class-Specific Request Codes".
const int kVcRequestCodeSetCur = 0x1;
// PU_POWER_LINE_FREQUENCY_CONTROL: Sec. A.9.5 "Processing Unit Control
// Selectors".
const int kPuPowerLineFrequencyControl = 0x5;
// Sec. 4.2.2.3.5 Power Line Frequency Control.
const int k50Hz = 1;
const int k60Hz = 2;
const int kPuPowerLineFrequencyControlCommandSize = 1;

// Addition to the IOUSB family of structures, with subtype and unit ID.
typedef struct IOUSBInterfaceDescriptor {
  IOUSBDescriptorHeader header;
  UInt8 bDescriptorSubType;
  UInt8 bUnitID;
} IOUSBInterfaceDescriptor;

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
    DLOG(ERROR) << "IOCreatePlugInInterfaceForService";
    return false;
  }
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin_ref(plugin);

  // Fetch the Device Interface from the plugin.
  HRESULT res = (*plugin)->QueryInterface(
      plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
      reinterpret_cast<LPVOID*>(device_interface));
  if (!SUCCEEDED(res) || !*device_interface) {
    DLOG(ERROR) << "QueryInterface, couldn't create interface to USB";
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
    DLOG(ERROR) << "Could not create an iterator to the device's interfaces.";
    return false;
  }
  base::mac::ScopedIOObject<io_iterator_t> iterator_ref(interface_iterator);

  // There should be just one interface matching the class-subclass desired.
  io_service_t found_interface;
  found_interface = IOIteratorNext(interface_iterator);
  if (!found_interface) {
    DLOG(ERROR) << "Could not find a Video-AVControl interface in the device.";
    return false;
  }
  base::mac::ScopedIOObject<io_service_t> found_interface_ref(found_interface);

  // Create a user side controller (i.e. a "plugin") for the found interface.
  SInt32 score;
  kr = IOCreatePlugInInterfaceForService(
      found_interface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID,
      video_control_interface, &score);
  if (kr != kIOReturnSuccess || !*video_control_interface) {
    DLOG(ERROR) << "IOCreatePlugInInterfaceForService";
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
  IOUSBInterfaceInterface** control_interface = NULL;
  HRESULT res =
      (*plugin_interface)
          ->QueryInterface(plugin_interface,
                           CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
                           reinterpret_cast<LPVOID*>(&control_interface));
  if (!SUCCEEDED(res) || !control_interface) {
    DLOG(ERROR) << "Couldn’t create control interface";
    return;
  }
  base::mac::ScopedIOPluginInterface<IOUSBInterfaceInterface>
      control_interface_ref(control_interface);

  // Find the device's unit ID presenting type 0x24 (kVcCsInterface) and
  // subtype 0x5 (kVcProcessingUnit). Inside this unit is where we find the
  // power line frequency removal setting, and this id is device dependent.
  int real_unit_id = -1;
  IOUSBDescriptorHeader* descriptor = NULL;
  IOUSBInterfaceDescriptor* cs_descriptor = NULL;
  IOUSBInterfaceInterface220** interface =
      reinterpret_cast<IOUSBInterfaceInterface220**>(control_interface);
  while ((descriptor = (*interface)
                           ->FindNextAssociatedDescriptor(interface, descriptor,
                                                          kUSBAnyDesc))) {
    cs_descriptor = reinterpret_cast<IOUSBInterfaceDescriptor*>(descriptor);
    if ((descriptor->bDescriptorType == kVcCsInterface) &&
        (cs_descriptor->bDescriptorSubType == kVcProcessingUnit)) {
      real_unit_id = cs_descriptor->bUnitID;
      break;
    }
  }
  DVLOG_IF(1, real_unit_id == -1)
      << "This USB device doesn't seem to have a "
      << " VC_PROCESSING_UNIT, anti-flicker not available";
  if (real_unit_id == -1)
    return;

  if ((*control_interface)->USBInterfaceOpen(control_interface) !=
      kIOReturnSuccess) {
    DLOG(ERROR) << "Unable to open control interface";
    return;
  }

  // Create the control request and launch it to the device's control interface.
  // Note how the wIndex needs the interface number OR'ed in the lowest bits.
  IOUSBDevRequest command;
  command.bmRequestType =
      USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
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
  DLOG_IF(ERROR, ret != kIOReturnSuccess) << "Anti-flicker control request"
                                          << " failed (0x" << std::hex << ret
                                          << "), unit id: " << real_unit_id;
  DVLOG_IF(1, ret == kIOReturnSuccess) << "Anti-flicker set to "
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

  // Compose a search dictionary with vendor and product ID.
  CFMutableDictionaryRef query_dictionary =
      IOServiceMatching(kIOUSBDeviceClassName);
  CFDictionarySetValue(
      query_dictionary, CFSTR(kUSBVendorName),
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vendor_id));
  CFDictionarySetValue(
      query_dictionary, CFSTR(kUSBProductName),
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &product_id));

  io_iterator_t usb_iterator;
  kern_return_t kr = IOServiceGetMatchingServices(
      kIOMasterPortDefault, query_dictionary, &usb_iterator);
  if (kr != kIOReturnSuccess) {
    DLOG(ERROR) << "No devices found with specified Vendor and Product ID.";
    return;
  }
  base::mac::ScopedIOObject<io_iterator_t> usb_iterator_ref(usb_iterator);

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

VideoCaptureDeviceMac::VideoCaptureDeviceMac(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : device_descriptor_(device_descriptor),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
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
  if (state_ != kIdle) {
    return;
  }

  client_ = std::move(client);
  if (device_descriptor_.capture_api == VideoCaptureApi::MACOSX_AVFOUNDATION)
    LogMessage("Using AVFoundation for device: " +
               device_descriptor_.display_name());

  NSString* deviceId =
      [NSString stringWithUTF8String:device_descriptor_.device_id.c_str()];

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
    std::string model_id = device_model.substr(kVidPidSize + 1);
    int vendor_id_as_int, model_id_as_int;
    if (base::HexStringToInt(base::StringPiece(vendor_id), &vendor_id_as_int) &&
        base::HexStringToInt(base::StringPiece(model_id), &model_id_as_int)) {
      SetAntiFlickerInUsbDevice(vendor_id_as_int, model_id_as_int,
                                GetPowerLineFrequency(params));
    }
  }

  if (![capture_device_ startCapture]) {
    SetErrorState(VideoCaptureError::kMacCouldNotStartCaptureDevice, FROM_HERE,
                  "Could not start capture device.");
    return;
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
  std::move(callback).Run(true);
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
  DLOG(ERROR) << __func__ << " error taking picture";
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

void VideoCaptureDeviceMac::SetErrorState(VideoCaptureError error,
                                          const base::Location& from_here,
                                          const std::string& reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  state_ = kError;
  client_->OnError(error, from_here, reason);
}

bool VideoCaptureDeviceMac::UpdateCaptureResolution() {
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
