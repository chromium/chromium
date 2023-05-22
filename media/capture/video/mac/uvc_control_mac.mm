// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/uvc_control_mac.h"

#include <IOKit/IOCFPlugIn.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/string_number_conversions.h"

namespace media {

namespace {

const unsigned int kRequestTimeoutInMilliseconds = 1000;

}

// Addition to the IOUSB family of structures, with subtype and unit ID.
// Sec. 3.7.2 "Class-Specific VC Interface Descriptor"
typedef struct VcCsInterfaceDescriptor {
  IOUSBDescriptorHeader header;
  UInt8 bDescriptorSubType;
  UInt8 bUnitID;
} VcCsInterfaceDescriptor;

// Sec. 3.7.2.5 "Processing Unit Descriptor"
typedef struct ProcessingUnitDescriptor {
  UInt8 bLength;
  UInt8 bDescriptorType;
  UInt8 bDescriptorSubType;
  UInt8 bUnitID;
  UInt8 bSourceID;
  UInt16 wMaxMultiplier;
  UInt8 bControlSize;  // Size of the bmControls field, in bytes.
  UInt8 bmControls[];  // A bit set to 1 indicates that the mentioned Control is
                       // supported for the video stream.
} __attribute__((packed)) ProcessingUnitDescriptor;

// Sec. 3.7.2.3 "Camera Terminal Descriptor"
typedef struct {
  UInt8 bLength;
  UInt8 bDescriptorType;
  UInt8 bDescriptorSubType;
  UInt8 bTerminalId;
  UInt16 wTerminalType;
  UInt8 bAssocTerminal;
  UInt8 iTerminal;
  UInt16 wObjectiveFocalLengthMin;
  UInt16 wObjectiveFocalLengthMax;
  UInt16 wOcularFocalLength;
  UInt8 bControlSize;  // Size of the bmControls field, in bytes.
  UInt8 bmControls[];  // A bit set to 1 indicates that the mentioned Control is
                       // supported for the video stream.
} __attribute__((packed)) CameraTerminalDescriptor;

static constexpr int kPuBrightnessAbsoluteControlBitIndex = 0;
static constexpr int kPuContrastAbsoluteControlBitIndex = 1;
static constexpr int kPuSaturationAbsoluteControlBitIndex = 3;
static constexpr int kPuSharpnessAbsoluteControlBitIndex = 4;
static constexpr int kPuWhiteBalanceTemperatureControlBitIndex = 5;
static constexpr int kPuPowerLineFrequencyControlBitIndex = 10;
static constexpr int kPuWhiteBalanceTemperatureAutoControlBitIndex = 12;

static constexpr int kCtAutoExposureModeControlBitIndex = 1;
static constexpr int kCtExposureTimeAbsoluteControlBitIndex = 3;
static constexpr int kCtFocusAbsoluteControlBitIndex = 5;
static constexpr int kCtZoomAbsoluteControlBitIndex = 9;
static constexpr int kCtPanTiltAbsoluteControlBitIndex = 11;
static constexpr int kCtFocusAutoControlBitIndex = 17;

static const base::flat_map<int, size_t> kProcessingUnitControlBitIndexes = {
    {uvc::kPuBrightnessAbsoluteControl, kPuBrightnessAbsoluteControlBitIndex},
    {uvc::kPuContrastAbsoluteControl, kPuContrastAbsoluteControlBitIndex},
    {uvc::kPuSaturationAbsoluteControl, kPuSaturationAbsoluteControlBitIndex},
    {uvc::kPuSharpnessAbsoluteControl, kPuSharpnessAbsoluteControlBitIndex},
    {uvc::kPuWhiteBalanceTemperatureControl,
     kPuWhiteBalanceTemperatureControlBitIndex},
    {uvc::kPuPowerLineFrequencyControl, kPuPowerLineFrequencyControlBitIndex},
    {uvc::kPuWhiteBalanceTemperatureAutoControl,
     kPuWhiteBalanceTemperatureAutoControlBitIndex},
};

static const base::flat_map<int, size_t> kCameraTerminalControlBitIndexes = {
    {uvc::kCtAutoExposureModeControl, kCtAutoExposureModeControlBitIndex},
    {uvc::kCtExposureTimeAbsoluteControl,
     kCtExposureTimeAbsoluteControlBitIndex},
    {uvc::kCtFocusAbsoluteControl, kCtFocusAbsoluteControlBitIndex},
    {uvc::kCtZoomAbsoluteControl, kCtZoomAbsoluteControlBitIndex},
    {uvc::kCtPanTiltAbsoluteControl, kCtPanTiltAbsoluteControlBitIndex},
    {uvc::kCtFocusAutoControl, kCtFocusAutoControlBitIndex},
};

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
// true if interface was found and passes it back in |device_interface|.
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
// found.
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

template <typename DescriptorType>
std::vector<uint8_t> ExtractControls(IOUSBDescriptorHeader* usb_descriptor) {
  auto* descriptor = reinterpret_cast<DescriptorType>(usb_descriptor);
  if (descriptor->bControlSize > 0) {
    NSData* data = [[NSData alloc] initWithBytes:&descriptor->bmControls[0]
                                          length:descriptor->bControlSize];
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>([data bytes]);
    return std::vector<uint8_t>(bytes, bytes + [data length]);
  }
  return std::vector<uint8_t>();
}

// Open the video class specific interface in a USB webcam identified by
// |device_model|. Returns interface when it is succcessfully opened.
static ScopedIOUSBInterfaceInterface OpenVideoClassSpecificControlInterface(
    std::string device_model,
    int descriptor_subtype,
    int& unit_id,
    std::vector<uint8_t>& controls) {
  if (device_model.length() <= 2 * media::kVidPidSize) {
    return ScopedIOUSBInterfaceInterface();
  }
  std::string vendor_id = device_model.substr(0, media::kVidPidSize);
  std::string product_id = device_model.substr(media::kVidPidSize + 1);
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

  // Find the device's unit ID presenting type kVcCsInterface and the descriptor
  // subtype.
  IOUSBDescriptorHeader* descriptor = nullptr;
  while ((descriptor = (*control_interface)
                           ->FindNextAssociatedDescriptor(
                               control_interface.get(), descriptor,
                               uvc::kVcCsInterface))) {
    auto* cs_descriptor =
        reinterpret_cast<VcCsInterfaceDescriptor*>(descriptor);
    if (cs_descriptor->bDescriptorSubType == descriptor_subtype) {
      unit_id = cs_descriptor->bUnitID;
      if (descriptor_subtype == uvc::kVcProcessingUnit) {
        controls = ExtractControls<ProcessingUnitDescriptor*>(descriptor);
      } else if (descriptor_subtype == uvc::kVcInputTerminal) {
        controls = ExtractControls<CameraTerminalDescriptor*>(descriptor);
      }
      break;
    }
  }

  VLOG_IF(1, unit_id == -1) << "This USB device doesn't seem to have a "
                            << descriptor_subtype << " descriptor subtype.";
  if (unit_id == -1) {
    return ScopedIOUSBInterfaceInterface();
  }

  IOReturn ret = (*control_interface)->USBInterfaceOpen(control_interface);
  if (ret != kIOReturnSuccess) {
    VLOG(1) << "Unable to open control interface";

    // Temporary additional debug logging for crbug.com/1270335
    VLOG_IF(1, base::mac::IsAtLeastOS12() && ret == kIOReturnExclusiveAccess)
        << "Camera USBInterfaceOpen failed with "
        << "kIOReturnExclusiveAccess";
    return ScopedIOUSBInterfaceInterface();
  }
  return control_interface;
}

UvcControl::UvcControl(std::string device_model, int descriptor_subtype)
    : descriptor_subtype_(descriptor_subtype) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "UvcControl::CreateUvcControl", "device_model", device_model,
               "descriptor_subtype", descriptor_subtype);
  interface_ = OpenVideoClassSpecificControlInterface(
      device_model, descriptor_subtype, unit_id_, controls_);
}

UvcControl::~UvcControl() {
  if (interface_) {
    (*interface_)->USBInterfaceClose(interface_);
  }
}

bool UvcControl::IsControlAvailable(int control_selector) const {
  if (!controls_.size()) {
    return false;
  }
  size_t bitIndex;
  if (descriptor_subtype_ == uvc::kVcProcessingUnit) {
    auto it = kProcessingUnitControlBitIndexes.find(control_selector);
    if (it == kProcessingUnitControlBitIndexes.end()) {
      return false;
    }
    bitIndex = it->second;
  } else if (descriptor_subtype_ == uvc::kVcInputTerminal) {
    auto it = kCameraTerminalControlBitIndexes.find(control_selector);
    if (it == kCameraTerminalControlBitIndexes.end()) {
      return false;
    }
    bitIndex = it->second;
  } else {
    return false;
  }
  UInt8 byteIndex = bitIndex / 8;
  if (byteIndex > controls_.size()) {
    return false;
  }
  return ((controls_[byteIndex] & (1 << bitIndex % 8)) != 0);
}

// Create an empty IOUSBDevRequestTO for a USB device to either set or get
// controls.
IOUSBDevRequestTO UvcControl::CreateEmptyCommand(
    int request_code,
    int endpoint_direction,
    int control_selector,
    int control_command_size) const {
  CHECK(interface_);
  CHECK((endpoint_direction == kUSBIn) || (endpoint_direction == kUSBOut));
  UInt8 interface_number;
  (*interface_)->GetInterfaceNumber(interface_, &interface_number);
  IOUSBDevRequestTO command;
  memset(&command, 0, sizeof(command));
  command.bmRequestType = USBmakebmRequestType(
      endpoint_direction, UInt8{kUSBClass}, UInt8{kUSBInterface});
  command.bRequest = request_code;
  command.wIndex = (unit_id_ << 8) | interface_number;
  command.wValue = (control_selector << 8);
  command.wLength = control_command_size;
  command.wLenDone = 0;
  command.noDataTimeout = kRequestTimeoutInMilliseconds;
  command.completionTimeout = kRequestTimeoutInMilliseconds;
  return command;
}

}  // namespace media
