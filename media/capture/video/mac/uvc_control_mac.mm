// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/mac/uvc_control_mac.h"

#include <IOKit/IOCFPlugIn.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/string_number_conversions.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

namespace {
const unsigned int kRequestTimeoutInMilliseconds = 1000;

BASE_FEATURE(kExposeAllUvcControls,
             "ExposeAllUvcControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

struct PanTilt {
  int32_t pan;
  int32_t tilt;
};

// Print the pan-tilt values. Used for friendly log messages.
::std::ostream& operator<<(::std::ostream& os, const PanTilt& pan_tilt) {
  return os << " pan: " << pan_tilt.pan << ", tilt " << pan_tilt.tilt;
}

// Update the control range and current value of pan-tilt control if
// possible.
static void MaybeUpdatePanTiltControlRange(UvcControl& uvc,
                                           mojom::Range* pan_range,
                                           mojom::Range* tilt_range) {
  PanTilt pan_tilt_max, pan_tilt_min, pan_tilt_step, pan_tilt_current;
  if (!uvc.GetControlMax<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_max,
                                  "pan-tilt") ||
      !uvc.GetControlMin<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_min,
                                  "pan-tilt") ||
      !uvc.GetControlStep<PanTilt>(uvc::kCtPanTiltAbsoluteControl,
                                   &pan_tilt_step, "pan-tilt") ||
      !uvc.GetControlCurrent<PanTilt>(uvc::kCtPanTiltAbsoluteControl,
                                      &pan_tilt_current, "pan-tilt")) {
    return;
  }
  pan_range->max = pan_tilt_max.pan;
  pan_range->min = pan_tilt_min.pan;
  pan_range->step = pan_tilt_step.pan;
  pan_range->current = pan_tilt_current.pan;
  tilt_range->max = pan_tilt_max.tilt;
  tilt_range->min = pan_tilt_min.tilt;
  tilt_range->step = pan_tilt_step.tilt;
  tilt_range->current = pan_tilt_current.tilt;
}

// Set pan and tilt values for a USB camera device.
static void SetPanTiltCurrent(UvcControl& uvc,
                              std::optional<int> pan,
                              std::optional<int> tilt) {
  DCHECK(pan.has_value() || tilt.has_value());

  PanTilt pan_tilt_current;
  if ((!pan.has_value() || !tilt.has_value()) &&
      !uvc.GetControlCurrent<PanTilt>(uvc::kCtPanTiltAbsoluteControl,
                                      &pan_tilt_current, "pan-tilt")) {
    return;
  }

  PanTilt pan_tilt_data;
  pan_tilt_data.pan =
      CFSwapInt32HostToLittle((int32_t)pan.value_or(pan_tilt_current.pan));
  pan_tilt_data.tilt =
      CFSwapInt32HostToLittle((int32_t)tilt.value_or(pan_tilt_current.tilt));

  uvc.SetControlCurrent<PanTilt>(uvc::kCtPanTiltAbsoluteControl, pan_tilt_data,
                                 "pan-tilt");
}

static bool IsNonEmptyRange(const mojom::RangePtr& range) {
  return range->min < range->max;
}

}  // namespace

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

static constexpr auto kProcessingUnitControlBitIndexes =
    base::MakeFixedFlatMap<int, size_t>(
        {{uvc::kPuBrightnessAbsoluteControl,
          kPuBrightnessAbsoluteControlBitIndex},
         {uvc::kPuContrastAbsoluteControl, kPuContrastAbsoluteControlBitIndex},
         {uvc::kPuSaturationAbsoluteControl,
          kPuSaturationAbsoluteControlBitIndex},
         {uvc::kPuSharpnessAbsoluteControl,
          kPuSharpnessAbsoluteControlBitIndex},
         {uvc::kPuWhiteBalanceTemperatureControl,
          kPuWhiteBalanceTemperatureControlBitIndex},
         {uvc::kPuPowerLineFrequencyControl,
          kPuPowerLineFrequencyControlBitIndex},
         {uvc::kPuWhiteBalanceTemperatureAutoControl,
          kPuWhiteBalanceTemperatureAutoControlBitIndex}});

static constexpr auto kCameraTerminalControlBitIndexes =
    base::MakeFixedFlatMap<int, size_t>({
        {uvc::kCtAutoExposureModeControl, kCtAutoExposureModeControlBitIndex},
        {uvc::kCtExposureTimeAbsoluteControl,
         kCtExposureTimeAbsoluteControlBitIndex},
        {uvc::kCtFocusAbsoluteControl, kCtFocusAbsoluteControlBitIndex},
        {uvc::kCtZoomAbsoluteControl, kCtZoomAbsoluteControlBitIndex},
        {uvc::kCtPanTiltAbsoluteControl, kCtPanTiltAbsoluteControlBitIndex},
        {uvc::kCtFocusAutoControl, kCtFocusAutoControlBitIndex},
    });

static bool FindDeviceWithVendorAndProductIds(int vendor_id,
                                              int product_id,
                                              io_iterator_t* usb_iterator) {
  // Compose a search dictionary with vendor and product ID.
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> query_dictionary(
      IOServiceMatching(kIOUSBDeviceClassName));
  CFDictionarySetValue(query_dictionary.get(), CFSTR(kUSBVendorName),
                       base::apple::NSToCFPtrCast(@(vendor_id)));
  CFDictionarySetValue(query_dictionary.get(), CFSTR(kUSBProductName),
                       base::apple::NSToCFPtrCast(@(product_id)));

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
  base::mac::ScopedIOPluginInterface<IOCFPlugInInterface> plugin;
  SInt32 score;  // Unused, but required for IOCreatePlugInInterfaceForService.
  kern_return_t kr = IOCreatePlugInInterfaceForService(
      usb_device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
      plugin.InitializeInto(), &score);
  if (kr != kIOReturnSuccess || !plugin) {
    VLOG(1) << "IOCreatePlugInInterfaceForService";
    return false;
  }

  // Fetch the Device Interface from the plugin.
  HRESULT res =
      (*plugin.get())
          ->QueryInterface(plugin.get(),
                           CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
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
  base::mac::ScopedIOObject<io_iterator_t> interface_iterator;
  IOUSBFindInterfaceRequest interface_request = {
      .bInterfaceClass = kUSBVideoInterfaceClass,
      .bInterfaceSubClass = kUSBVideoControlSubClass,
      .bInterfaceProtocol = kIOUSBFindInterfaceDontCare,
      .bAlternateSetting = kIOUSBFindInterfaceDontCare};
  kern_return_t kr =
      (*device_interface)
          ->CreateInterfaceIterator(device_interface, &interface_request,
                                    interface_iterator.InitializeInto());
  if (kr != kIOReturnSuccess) {
    VLOG(1) << "Could not create an iterator to the device's interfaces.";
    return false;
  }

  // There should be just one interface matching the class-subclass desired.
  base::mac::ScopedIOObject<io_service_t> found_interface(
      IOIteratorNext(interface_iterator.get()));
  if (!found_interface) {
    VLOG(1) << "Could not find a Video-AVControl interface in the device.";
    return false;
  }

  // Create a user side controller (i.e. a "plugin") for the found interface.
  SInt32 score;
  kr = IOCreatePlugInInterfaceForService(
      found_interface.get(), kIOUSBInterfaceUserClientTypeID,
      kIOCFPlugInInterfaceID, video_control_interface, &score);
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
    const uint8_t* bytes =
        reinterpret_cast<const uint8_t*>(&descriptor->bmControls[0]);
    const size_t length = descriptor->bControlSize;
    return std::vector<uint8_t>(bytes, bytes + length);
  }
  return std::vector<uint8_t>();
}

// Open the video class specific interface in a USB webcam identified by
// |device_model|. Returns interface when it is successfully opened.
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

  while (io_service_t usb_device = IOIteratorNext(usb_iterator.get())) {
    base::mac::ScopedIOObject<io_service_t> usb_device_ref(usb_device);
    base::mac::ScopedIOPluginInterface<IOUSBDeviceInterface> device_interface;

    if (!FindDeviceInterfaceInUsbDevice(vendor_id_as_int, product_id_as_int,
                                        usb_device,
                                        device_interface.InitializeInto())) {
      continue;
    }

    if (FindVideoControlInterfaceInDeviceInterface(
            device_interface.get(), video_control_interface.InitializeInto())) {
      break;
    }
  }

  if (!video_control_interface) {
    return ScopedIOUSBInterfaceInterface();
  }

  // Create the control interface for the found plugin, and release
  // the intermediate plugin.
  ScopedIOUSBInterfaceInterface control_interface;
  HRESULT res =
      (*video_control_interface.get())
          ->QueryInterface(
              video_control_interface.get(),
              CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID220),
              reinterpret_cast<LPVOID*>(control_interface.InitializeInto()));
  if (!SUCCEEDED(res) || !control_interface) {
    VLOG(1) << "Couldn’t get control interface";
    return ScopedIOUSBInterfaceInterface();
  }

  // Find the device's unit ID presenting type kVcCsInterface and the descriptor
  // subtype.
  IOUSBDescriptorHeader* descriptor = nullptr;
  while ((descriptor = (*control_interface.get())
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

  IOReturn ret =
      (*control_interface.get())->USBInterfaceOpen(control_interface.get());
  if (ret != kIOReturnSuccess) {
    VLOG(1) << "Unable to open control interface";

    // Temporary additional debug logging for crbug.com/1270335
    VLOG_IF(1, base::mac::MacOSMajorVersion() >= 12 &&
                   ret == kIOReturnExclusiveAccess)
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
    (*interface_.get())->USBInterfaceClose(interface_.get());
  }
}
// static
void UvcControl::SetPowerLineFrequency(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    const VideoCaptureParams& params) {
  PowerLineFrequency frequency =
      VideoCaptureDevice::GetPowerLineFrequency(params);
  if (frequency != PowerLineFrequency::kDefault) {
    // Try setting the power line frequency removal (anti-flicker). The built-in
    // cameras are normally suspended so the configuration must happen right
    // before starting capture and during configuration.
    const std::string device_model = GetDeviceModelId(
        device_descriptor.device_id, device_descriptor.capture_api,
        device_descriptor.transport_type);
    if (device_model.length() > 2 * kVidPidSize) {
      if (UvcControl uvc(device_model, uvc::kVcProcessingUnit); uvc.Good()) {
        int power_line_flag_value =
            (frequency == PowerLineFrequency::k50Hz) ? uvc::k50Hz : uvc::k60Hz;
        uvc.SetControlCurrent<uint8_t>(uvc::kPuPowerLineFrequencyControl,
                                       power_line_flag_value,
                                       "power line frequency");
      }
    }
  }
}

// static
void UvcControl::GetPhotoState(
    media::mojom::PhotoStatePtr& photo_state,
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  const std::string device_model = GetDeviceModelId(
      device_descriptor.device_id, device_descriptor.capture_api,
      device_descriptor.transport_type);
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    photo_state->current_focus_mode = mojom::MeteringMode::NONE;
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kCtFocusAbsoluteControl,
                                          photo_state->focus_distance.get(),
                                          "focus");
    if (IsNonEmptyRange(photo_state->focus_distance)) {
      photo_state->supported_focus_modes.push_back(mojom::MeteringMode::MANUAL);
    }
    bool current_auto_focus;
    if (uvc.GetControlCurrent<bool>(uvc::kCtFocusAutoControl,

                                    &current_auto_focus, "focus auto")) {
      photo_state->current_focus_mode = current_auto_focus
                                            ? mojom::MeteringMode::CONTINUOUS
                                            : mojom::MeteringMode::MANUAL;
      photo_state->supported_focus_modes.push_back(
          mojom::MeteringMode::CONTINUOUS);
    }

    photo_state->current_exposure_mode = mojom::MeteringMode::NONE;
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kCtExposureTimeAbsoluteControl,
                                          photo_state->exposure_time.get(),
                                          "exposure time");
    if (IsNonEmptyRange(photo_state->exposure_time)) {
      photo_state->supported_exposure_modes.push_back(
          mojom::MeteringMode::MANUAL);
    }
    uint8_t current_auto_exposure;
    if (uvc.GetControlCurrent<uint8_t>(uvc::kCtAutoExposureModeControl,
                                       &current_auto_exposure,
                                       "auto-exposure mode")) {
      if (current_auto_exposure & uvc::kExposureManual ||
          current_auto_exposure & uvc::kExposureShutterPriority) {
        photo_state->current_exposure_mode = mojom::MeteringMode::MANUAL;
      } else {
        photo_state->current_exposure_mode = mojom::MeteringMode::CONTINUOUS;
      }
      photo_state->supported_exposure_modes.push_back(
          mojom::MeteringMode::CONTINUOUS);
    }
  }
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal); uvc.Good()) {
    MaybeUpdatePanTiltControlRange(uvc, photo_state->pan.get(),
                                   photo_state->tilt.get());

    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kCtZoomAbsoluteControl,
                                          photo_state->zoom.get(), "zoom");
  }
  if (UvcControl uvc(device_model, uvc::kVcProcessingUnit);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    uvc.MaybeUpdateControlRange<int16_t>(uvc::kPuBrightnessAbsoluteControl,
                                         photo_state->brightness.get(),
                                         "brightness");
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kPuContrastAbsoluteControl,
                                          photo_state->contrast.get(),
                                          "contrast");
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kPuSaturationAbsoluteControl,
                                          photo_state->saturation.get(),
                                          "saturation");
    uvc.MaybeUpdateControlRange<uint16_t>(uvc::kPuSharpnessAbsoluteControl,
                                          photo_state->sharpness.get(),
                                          "sharpness");

    photo_state->current_white_balance_mode = mojom::MeteringMode::NONE;
    uvc.MaybeUpdateControlRange<uint16_t>(
        uvc::kPuWhiteBalanceTemperatureControl,
        photo_state->color_temperature.get(), "white balance temperature");
    if (IsNonEmptyRange(photo_state->color_temperature)) {
      photo_state->supported_white_balance_modes.push_back(
          mojom::MeteringMode::MANUAL);
    }
    bool current_auto_white_balance;
    if (uvc.GetControlCurrent<bool>(uvc::kPuWhiteBalanceTemperatureAutoControl,
                                    &current_auto_white_balance,
                                    "white balance temperature auto")) {
      photo_state->current_white_balance_mode =
          current_auto_white_balance ? mojom::MeteringMode::CONTINUOUS
                                     : mojom::MeteringMode::MANUAL;
      photo_state->supported_white_balance_modes.push_back(
          mojom::MeteringMode::CONTINUOUS);
    }
  }
}

// static
void UvcControl::SetPhotoState(
    mojom::PhotoSettingsPtr& settings,
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  const std::string device_model = GetDeviceModelId(
      device_descriptor.device_id, device_descriptor.capture_api,
      device_descriptor.transport_type);
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    if (settings->has_focus_mode &&
        (settings->focus_mode == mojom::MeteringMode::CONTINUOUS ||
         settings->focus_mode == mojom::MeteringMode::MANUAL)) {
      int focus_auto =
          (settings->focus_mode == mojom::MeteringMode::CONTINUOUS);
      uvc.SetControlCurrent<bool>(uvc::kCtFocusAutoControl, focus_auto,
                                  "focus auto");
    }
    if (settings->has_focus_distance) {
      uvc.SetControlCurrent<uint16_t>(uvc::kCtFocusAbsoluteControl,
                                      settings->focus_distance, "focus");
    }
    if (settings->has_exposure_mode &&
        (settings->exposure_mode == mojom::MeteringMode::CONTINUOUS ||
         settings->exposure_mode == mojom::MeteringMode::MANUAL)) {
      int auto_exposure_mode =
          settings->exposure_mode == mojom::MeteringMode::CONTINUOUS
              ? uvc::kExposureAperturePriority
              : uvc::kExposureManual;
      uvc.SetControlCurrent<uint8_t>(uvc::kCtAutoExposureModeControl,
                                     auto_exposure_mode, "auto-exposure mode");
    }
    if (settings->has_exposure_time) {
      uvc.SetControlCurrent<uint16_t>(uvc::kCtExposureTimeAbsoluteControl,
                                      settings->exposure_time, "exposure time");
    }
  }
  if (UvcControl uvc(device_model, uvc::kVcInputTerminal); uvc.Good()) {
    if (settings->has_pan || settings->has_tilt) {
      SetPanTiltCurrent(
          uvc,
          settings->has_pan ? std::make_optional(settings->pan) : std::nullopt,
          settings->has_tilt ? std::make_optional(settings->tilt)
                             : std::nullopt);
    }
    if (settings->has_zoom) {
      uvc.SetControlCurrent<uint16_t>(uvc::kCtZoomAbsoluteControl,
                                      settings->zoom, "zoom");
    }
  }
  if (UvcControl uvc(device_model, uvc::kVcProcessingUnit);
      uvc.Good() && base::FeatureList::IsEnabled(kExposeAllUvcControls)) {
    if (settings->has_brightness) {
      uvc.SetControlCurrent<int16_t>(uvc::kPuBrightnessAbsoluteControl,
                                     settings->brightness, "brightness");
    }
    if (settings->has_contrast) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuContrastAbsoluteControl,
                                      settings->contrast, "contrast");
    }
    if (settings->has_saturation) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuSaturationAbsoluteControl,
                                      settings->saturation, "saturation");
    }
    if (settings->has_sharpness) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuSharpnessAbsoluteControl,
                                      settings->sharpness, "sharpness");
    }
    if (settings->has_white_balance_mode &&
        (settings->white_balance_mode == mojom::MeteringMode::CONTINUOUS ||
         settings->white_balance_mode == mojom::MeteringMode::MANUAL)) {
      int white_balance_temperature_auto =
          (settings->white_balance_mode == mojom::MeteringMode::CONTINUOUS);
      uvc.SetControlCurrent<uint8_t>(uvc::kPuWhiteBalanceTemperatureAutoControl,
                                     white_balance_temperature_auto,
                                     "white balance temperature auto");
    }
    if (settings->has_color_temperature) {
      uvc.SetControlCurrent<uint16_t>(uvc::kPuWhiteBalanceTemperatureControl,
                                      settings->color_temperature,
                                      "white balance temperature");
    }
  }
}

// static
std::string UvcControl::GetDeviceModelId(
    const std::string& device_id,
    VideoCaptureApi capture_api,
    VideoCaptureTransportType transport_type) {
  // Skip the AVFoundation's not USB nor built-in devices.
  if (capture_api == VideoCaptureApi::MACOSX_AVFOUNDATION &&
      transport_type != VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN) {
    return "";
  }
  if (capture_api == VideoCaptureApi::MACOSX_DECKLINK) {
    return "";
  }
  // Both PID and VID are 4 characters.
  if (device_id.size() < 2 * kVidPidSize) {
    return "";
  }

  // The last characters of device id is a concatenation of VID and then PID.
  const size_t vid_location = device_id.size() - 2 * kVidPidSize;
  std::string id_vendor = device_id.substr(vid_location, kVidPidSize);
  const size_t pid_location = device_id.size() - kVidPidSize;
  std::string id_product = device_id.substr(pid_location, kVidPidSize);

  return id_vendor + ":" + id_product;
}

// Check if the video capture device supports pan, tilt and zoom controls.
// static
VideoCaptureControlSupport UvcControl::GetControlSupport(
    const std::string& device_model) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceApple::GetControlSupport");
  VideoCaptureControlSupport control_support;

  UvcControl uvc(device_model, uvc::kVcInputTerminal);
  if (!uvc.Good()) {
    return control_support;
  }

  uint16_t zoom_max, zoom_min = 0;
  if (uvc.GetControlMax<uint16_t>(uvc::kCtZoomAbsoluteControl, &zoom_max,
                                  "zoom") &&
      uvc.GetControlMin<uint16_t>(uvc::kCtZoomAbsoluteControl, &zoom_min,
                                  "zoom") &&
      zoom_min < zoom_max) {
    control_support.zoom = true;
  }
  PanTilt pan_tilt_max, pan_tilt_min;
  if (uvc.GetControlMax<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_max,
                                 "pan-tilt") &&
      uvc.GetControlMin<PanTilt>(uvc::kCtPanTiltAbsoluteControl, &pan_tilt_min,
                                 "pan-tilt")) {
    if (pan_tilt_min.pan < pan_tilt_max.pan) {
      control_support.pan = true;
    }
    if (pan_tilt_min.tilt < pan_tilt_max.tilt) {
      control_support.tilt = true;
    }
  }

  return control_support;
}

bool UvcControl::IsControlAvailable(int control_selector) const {
  if (!controls_.size()) {
    return false;
  }
  size_t bitIndex;
  if (descriptor_subtype_ == uvc::kVcProcessingUnit) {
    const auto it = kProcessingUnitControlBitIndexes.find(control_selector);
    if (it == kProcessingUnitControlBitIndexes.end()) {
      return false;
    }
    bitIndex = it->second;
  } else if (descriptor_subtype_ == uvc::kVcInputTerminal) {
    const auto it = kCameraTerminalControlBitIndexes.find(control_selector);
    if (it == kCameraTerminalControlBitIndexes.end()) {
      return false;
    }
    bitIndex = it->second;
  } else {
    return false;
  }
  UInt8 byteIndex = bitIndex / 8;
  if (byteIndex >= controls_.size()) {
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
  (*interface_.get())->GetInterfaceNumber(interface_.get(), &interface_number);
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
