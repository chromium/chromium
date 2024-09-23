// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/core_audio_util_mac.h"

#include <IOKit/audio/IOAudioTypes.h>

#include <utility>

#include "base/apple/osstatus_logging.h"
#include "base/containers/heap_array.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/audio/apple/scoped_audio_unit.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {
namespace core_audio_mac {
namespace {

// Kill switch in case anything explodes. Remove after M132.
BASE_FEATURE(kExcludeDeviceLatencyFromTotalLatency,
             "ExcludeDeviceLatencyFromTotalLatency",
             base::FEATURE_ENABLED_BY_DEFAULT);

AudioObjectPropertyScope InputOutputScope(bool is_input) {
  return is_input ? kAudioObjectPropertyScopeInput
                  : kAudioObjectPropertyScopeOutput;
}

void RecordCompositionPropertyIsNull(bool is_null) {
  base::UmaHistogramBoolean(
      "Media.Audio.Mac.AggregateDeviceCompositionPropertyIsNull", is_null);
}

std::optional<std::string> GetDeviceStringProperty(
    AudioObjectID device_id,
    AudioObjectPropertySelector property_selector) {
  CFStringRef property_value = nullptr;
  UInt32 size = sizeof(property_value);
  AudioObjectPropertyAddress property_address = {
      property_selector, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};

  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, &property_value);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Failed to read string property " << property_selector
        << " for device " << device_id;
    return std::nullopt;
  }

  if (!property_value)
    return std::nullopt;

  std::string device_property = base::SysCFStringRefToUTF8(property_value);
  CFRelease(property_value);

  return device_property;
}

std::optional<uint32_t> GetDeviceUint32Property(
    AudioObjectID device_id,
    AudioObjectPropertySelector property_selector,
    AudioObjectPropertyScope property_scope) {
  AudioObjectPropertyAddress property_address = {
      property_selector, property_scope, kAudioObjectPropertyElementMain};
  UInt32 property_value;
  UInt32 size = sizeof(property_value);
  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, &property_value);
  if (result != noErr)
    return std::nullopt;

  return property_value;
}

uint32_t GetDevicePropertySize(AudioObjectID device_id,
                               AudioObjectPropertySelector property_selector,
                               AudioObjectPropertyScope property_scope) {
  AudioObjectPropertyAddress property_address = {
      property_selector, property_scope, kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  OSStatus result = AudioObjectGetPropertyDataSize(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Failed to read size of property " << property_selector
        << " for device " << device_id;
    return 0;
  }
  return size;
}

std::vector<AudioObjectID> GetAudioObjectIDs(
    AudioObjectID audio_object_id,
    AudioObjectPropertySelector property_selector) {
  AudioObjectPropertyAddress property_address = {
      property_selector, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  OSStatus result = AudioObjectGetPropertyDataSize(
      audio_object_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Failed to read size of property " << property_selector
        << " for device/object " << audio_object_id;
    return {};
  }

  if (size == 0)
    return {};

  size_t device_count = size / sizeof(AudioObjectID);
  // Get the array of device ids for all the devices, which includes both
  // input devices and output devices.
  std::vector<AudioObjectID> device_ids(device_count);
  result = AudioObjectGetPropertyData(
      audio_object_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, device_ids.data());
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Failed to read object IDs from property " << property_selector
        << " for device/object " << audio_object_id;
    return {};
  }

  return device_ids;
}

std::optional<std::string> GetDeviceName(AudioObjectID device_id) {
  return GetDeviceStringProperty(device_id, kAudioObjectPropertyName);
}

std::optional<std::string> GetDeviceModel(AudioObjectID device_id) {
  return GetDeviceStringProperty(device_id, kAudioDevicePropertyModelUID);
}

bool ModelContainsVidPid(const std::string& model) {
  return model.size() > 10 && model[model.size() - 5] == ':' &&
         model[model.size() - 10] == ':';
}

std::string UsbVidPidFromModel(const std::string& model) {
  return ModelContainsVidPid(model)
             ? base::ToLowerASCII(model.substr(model.size() - 9))
             : std::string();
}

std::string TransportTypeToString(uint32_t transport_type) {
  switch (transport_type) {
    case kAudioDeviceTransportTypeBuiltIn:
      return "Built-in";
    case kAudioDeviceTransportTypeAggregate:
      return "Aggregate";
    case kAudioDeviceTransportTypeAutoAggregate:
      return "AutoAggregate";
    case kAudioDeviceTransportTypeVirtual:
      return "Virtual";
    case kAudioDeviceTransportTypePCI:
      return "PCI";
    case kAudioDeviceTransportTypeUSB:
      return "USB";
    case kAudioDeviceTransportTypeFireWire:
      return "FireWire";
    case kAudioDeviceTransportTypeBluetooth:
      return "Bluetooth";
    case kAudioDeviceTransportTypeBluetoothLE:
      return "Bluetooth LE";
    case kAudioDeviceTransportTypeHDMI:
      return "HDMI";
    case kAudioDeviceTransportTypeDisplayPort:
      return "DisplayPort";
    case kAudioDeviceTransportTypeAirPlay:
      return "AirPlay";
    case kAudioDeviceTransportTypeAVB:
      return "AVB";
    case kAudioDeviceTransportTypeThunderbolt:
      return "Thunderbolt";
    case kAudioDeviceTransportTypeUnknown:
    default:
      return std::string();
  }
}

std::optional<std::string> TranslateDeviceSource(AudioObjectID device_id,
                                                 UInt32 source_id,
                                                 bool is_input) {
  CFStringRef source_name = nullptr;
  AudioValueTranslation translation;
  translation.mInputData = &source_id;
  translation.mInputDataSize = sizeof(source_id);
  translation.mOutputData = &source_name;
  translation.mOutputDataSize = sizeof(source_name);

  UInt32 translation_size = sizeof(AudioValueTranslation);
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyDataSourceNameForIDCFString,
      InputOutputScope(is_input), kAudioObjectPropertyElementMain};

  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &translation_size, &translation);
  if (result)
    return std::nullopt;

  std::string ret = base::SysCFStringRefToUTF8(source_name);
  CFRelease(source_name);

  return ret;
}

}  // namespace

std::vector<AudioObjectID> GetAllAudioDeviceIDs() {
  return GetAudioObjectIDs(kAudioObjectSystemObject,
                           kAudioHardwarePropertyDevices);
}

std::vector<AudioObjectID> GetRelatedDeviceIDs(AudioObjectID device_id) {
  return GetAudioObjectIDs(device_id, kAudioDevicePropertyRelatedDevices);
}

std::optional<std::string> GetDeviceUniqueID(AudioObjectID device_id) {
  return GetDeviceStringProperty(device_id, kAudioDevicePropertyDeviceUID);
}

std::optional<std::string> GetDeviceLabel(AudioObjectID device_id,
                                          bool is_input) {
  std::optional<std::string> device_label;
  std::optional<uint32_t> source = GetDeviceSource(device_id, is_input);
  if (source) {
    device_label = TranslateDeviceSource(device_id, *source, is_input);
  }

  if (!device_label) {
    device_label = GetDeviceName(device_id);
    if (!device_label)
      return std::nullopt;
  }

  std::string suffix;
  std::optional<uint32_t> transport_type = GetDeviceTransportType(device_id);
  if (transport_type) {
    if (*transport_type == kAudioDeviceTransportTypeUSB) {
      std::optional<std::string> model = GetDeviceModel(device_id);
      if (model) {
        suffix = UsbVidPidFromModel(*model);
      }
    } else {
      suffix = TransportTypeToString(*transport_type);
    }
  }

  DCHECK(device_label);
  if (!suffix.empty())
    *device_label += " (" + suffix + ")";

  return device_label;
}

uint32_t GetNumStreams(AudioObjectID device_id, bool is_input) {
  return GetDevicePropertySize(device_id, kAudioDevicePropertyStreams,
                               InputOutputScope(is_input));
}

std::optional<uint32_t> GetDeviceSource(AudioObjectID device_id,
                                        bool is_input) {
  return GetDeviceUint32Property(device_id, kAudioDevicePropertyDataSource,
                                 InputOutputScope(is_input));
}

std::optional<uint32_t> GetDeviceTransportType(AudioObjectID device_id) {
  return GetDeviceUint32Property(device_id, kAudioDevicePropertyTransportType,
                                 kAudioObjectPropertyScopeGlobal);
}

bool IsPrivateAggregateDevice(AudioObjectID device_id) {
  // Don't try to access aggregate device properties unless |device_id| is
  // really an aggregate device.
  if (GetDeviceTransportType(device_id) != kAudioDeviceTransportTypeAggregate)
    return false;

  const AudioObjectPropertyAddress property_address = {
      kAudioAggregateDevicePropertyComposition, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  CFDictionaryRef dictionary = nullptr;
  UInt32 size = sizeof(dictionary);
  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, &dictionary);

  if (result != noErr) {
    OSSTATUS_LOG(WARNING, result) << "Failed to read property "
                                  << kAudioAggregateDevicePropertyComposition
                                  << " for device " << device_id;
    return false;
  }

  // It is possible that though the result was successful, the dictionary
  // might still be null.
  if (!dictionary) {
    RecordCompositionPropertyIsNull(/*is_null=*/true);
    DLOG(WARNING) << "Property " << kAudioAggregateDevicePropertyComposition
                  << " is null for device " << device_id;
    return false;
  }

  CHECK_EQ(CFGetTypeID(dictionary), CFDictionaryGetTypeID());
  RecordCompositionPropertyIsNull(/*is_null=*/false);
  bool is_private = false;
  CFTypeRef value = CFDictionaryGetValue(
      dictionary, CFSTR(kAudioAggregateDeviceIsPrivateKey));

  if (value && CFGetTypeID(value) == CFNumberGetTypeID()) {
    int number = 0;
    if (CFNumberGetValue(reinterpret_cast<CFNumberRef>(value), kCFNumberIntType,
                         &number)) {
      is_private = number != 0;
    }
  }
  CFRelease(dictionary);

  return is_private;
}

bool IsInputDevice(AudioObjectID device_id) {
  std::vector<AudioObjectID> streams =
      GetAudioObjectIDs(device_id, kAudioDevicePropertyStreams);

  int num_undefined_input_streams = 0;
  int num_defined_input_streams = 0;
  int num_output_streams = 0;

  for (auto stream_id : streams) {
    auto direction =
        GetDeviceUint32Property(stream_id, kAudioStreamPropertyDirection,
                                kAudioObjectPropertyScopeGlobal);
    if (!direction.has_value())
      continue;
    const UInt32 kDirectionOutput = 0;
    const UInt32 kDirectionInput = 1;
    if (direction == kDirectionOutput) {
      ++num_output_streams;
    } else if (direction == kDirectionInput) {
      // Filter input streams based on what terminal it claims to be attached
      // to. Note that INPUT_UNDEFINED comes from a set of terminals declared
      // in IOKit. CoreAudio defines a number of terminals in
      // AudioHardwareBase.h but none of them match any of the values I've
      // seen used in practice, though I've only tested a few devices.
      auto terminal =
          GetDeviceUint32Property(stream_id, kAudioStreamPropertyTerminalType,
                                  kAudioObjectPropertyScopeGlobal);
      if (terminal.has_value() && terminal == INPUT_UNDEFINED) {
        ++num_undefined_input_streams;
      } else {
        ++num_defined_input_streams;
      }
    }
  }

  // I've only seen INPUT_UNDEFINED introduced by the VoiceProcessing AudioUnit,
  // but to err on the side of caution, let's allow a device with only undefined
  // input streams and no output streams as well.
  return num_defined_input_streams > 0 ||
         (num_undefined_input_streams > 0 && num_output_streams == 0);
}

bool IsOutputDevice(AudioObjectID device_id) {
  return GetNumStreams(device_id, false) > 0;
}

// static
base::TimeDelta GetHardwareLatency(AudioUnit audio_unit,
                                   AudioDeviceID device_id,
                                   AudioObjectPropertyScope scope,
                                   int sample_rate,
                                   bool is_input) {
  if (!audio_unit || device_id == kAudioObjectUnknown) {
    DLOG(WARNING) << "Audio unit object is NULL or device ID is unknown";
    return base::TimeDelta();
  }

  // Total hardware latency is calculated as the sum of different values. See
  // https://lists.apple.com/archives/coreaudio-api/2017/Jul/msg00035.html
  //
  // Some of that info is out of date though. As of July 2024, Apple indicates
  // that device latency is already included in kAudioUnitProperty_Latency.

  // Get audio unit latency.
  Float64 audio_unit_latency_sec = 0.0;
  UInt32 size = sizeof(audio_unit_latency_sec);
  OSStatus result = AudioUnitGetProperty(
      audio_unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global,
      AUElement::OUTPUT, &audio_unit_latency_sec, &size);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "Could not get audio unit latency";

  // Get audio device latency.
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyLatency, scope, kAudioObjectPropertyElementMain};
  UInt32 device_latency_frames = 0;
  size = sizeof(device_latency_frames);
  result = AudioObjectGetPropertyData(device_id, &property_address, 0, nullptr,
                                      &size, &device_latency_frames);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "Could not get audio device latency.";

  // Retrieve stream ids and take the stream latency from the first stream.
  // There may be multiple streams with different latencies, but since we're
  // likely using this delay information for a/v sync we must choose one of
  // them; Apple recommends just taking the first entry.
  //
  // TODO(dalecurtis): Refactor all these "get data size" + "get data" calls
  // into a common utility function that just returns a std::unique_ptr.
  UInt32 stream_latency_frames = 0;
  property_address.mSelector = kAudioDevicePropertyStreams;
  result = AudioObjectGetPropertyDataSize(device_id, &property_address, 0,
                                          nullptr, &size);
  if (result == noErr && size >= sizeof(AudioStreamID)) {
    auto stream_id_storage = base::HeapArray<uint8_t>::Uninit(size);
    AudioStreamID* stream_ids =
        reinterpret_cast<AudioStreamID*>(stream_id_storage.data());
    result = AudioObjectGetPropertyData(device_id, &property_address, 0,
                                        nullptr, &size, stream_ids);
    if (result == noErr) {
      property_address.mSelector = kAudioStreamPropertyLatency;
      size = sizeof(stream_latency_frames);
      result =
          AudioObjectGetPropertyData(stream_ids[0], &property_address, 0,
                                     nullptr, &size, &stream_latency_frames);
      OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
          << "Could not get stream latency for stream #0.";
    } else {
      OSSTATUS_DLOG(WARNING, result)
          << "Could not get audio device stream ids.";
    }
  } else {
    OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
        << "Could not get audio device stream ids size.";
  }

  const base::TimeDelta audio_unit_latency =
      base::Seconds(audio_unit_latency_sec);
  const base::TimeDelta device_latency =
      AudioTimestampHelper::FramesToTime(device_latency_frames, sample_rate);
  const base::TimeDelta stream_latency =
      AudioTimestampHelper::FramesToTime(stream_latency_frames, sample_rate);
  const base::TimeDelta total_latency =
      audio_unit_latency +
      (base::FeatureList::IsEnabled(kExcludeDeviceLatencyFromTotalLatency)
           ? base::TimeDelta()
           : device_latency) +
      stream_latency;

  // This function is not currently not called on iOS, but guard against an
  // accidental future change drastically changing these metrics.
#if BUILDFLAG(IS_MAC)
  const std::string uma_name = base::StringPrintf(
      "Media.Audio.Mac.HardwareLatency.%s", is_input ? "Input" : "Output");

  base::UmaHistogramTimes(base::StringPrintf("%s.AudioUnit", uma_name.c_str()),
                          audio_unit_latency);
  base::UmaHistogramTimes(base::StringPrintf("%s.Device", uma_name.c_str()),
                          device_latency);
  base::UmaHistogramTimes(base::StringPrintf("%s.Stream", uma_name.c_str()),
                          stream_latency);
  base::UmaHistogramTimes(base::StringPrintf("%s.Total", uma_name.c_str()),
                          total_latency);
#endif

  return total_latency;
}

}  // namespace core_audio_mac
}  // namespace media
