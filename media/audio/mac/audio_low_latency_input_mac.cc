// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/audio/mac/audio_low_latency_input_mac.h"

#include <CoreAudio/AudioHardware.h>
#include <CoreServices/CoreServices.h>
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_mach_port.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/mac/core_audio_util_mac.h"
#include "media/audio/mac/scoped_audio_unit.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/data_buffer.h"

namespace {
extern "C" {
// See:
// https://trac.webkit.org/browser/webkit/trunk/Source/WebCore/PAL/pal/spi/cf/CoreAudioSPI.h?rev=228264
OSStatus AudioDeviceDuck(AudioDeviceID inDevice,
                         Float32 inDuckedLevel,
                         const AudioTimeStamp* __nullable inStartTime,
                         Float32 inRampDuration) __attribute__((weak_import));
}

void UndoDucking(AudioDeviceID output_device_id) {
  if (AudioDeviceDuck != nullptr) {
    // Ramp the volume back up over half a second.
    AudioDeviceDuck(output_device_id, 1.0, nullptr, 0.5);
  }
}

}  // namespace

namespace media {

// Number of blocks of buffers used in the |fifo_|.
const int kNumberOfBlocksBufferInFifo = 2;

// Max length of sequence of TooManyFramesToProcessError errors.
// The stream will be stopped as soon as this time limit is passed.
const int kMaxErrorTimeoutInSeconds = 1;

// A one-shot timer is created and started in Start() and it triggers
// CheckInputStartupSuccess() after this amount of time. UMA stats marked
// Media.Audio.InputStartupSuccessMac is then updated where true is added
// if input callbacks have started, and false otherwise.
const int kInputCallbackStartTimeoutInSeconds = 5;

// Returns true if the format flags in |format_flags| has the "non-interleaved"
// flag (kAudioFormatFlagIsNonInterleaved) cleared (set to 0).
static bool FormatIsInterleaved(UInt32 format_flags) {
  return !(format_flags & kAudioFormatFlagIsNonInterleaved);
}

// Converts the 32-bit non-terminated 4 byte string into an std::string.
// Example: code=1735354734 <=> 'goin' <=> kAudioDevicePropertyDeviceIsRunning.
static std::string FourCharFormatCodeToString(UInt32 code) {
  char code_string[5];
  // Converts a 32-bit integer from the host’s native byte order to big-endian.
  UInt32 code_id = CFSwapInt32HostToBig(code);
  bcopy(&code_id, code_string, 4);
  code_string[4] = '\0';
  return std::string(code_string);
}

static std::ostream& operator<<(std::ostream& os,
                                const AudioStreamBasicDescription& format) {
  std::string format_string = FourCharFormatCodeToString(format.mFormatID);
  os << "sample rate       : " << format.mSampleRate << std::endl
     << "format ID         : " << format_string << std::endl
     << "format flags      : " << format.mFormatFlags << std::endl
     << "bytes per packet  : " << format.mBytesPerPacket << std::endl
     << "frames per packet : " << format.mFramesPerPacket << std::endl
     << "bytes per frame   : " << format.mBytesPerFrame << std::endl
     << "channels per frame: " << format.mChannelsPerFrame << std::endl
     << "bits per channel  : " << format.mBitsPerChannel << std::endl
     << "reserved          : " << format.mReserved << std::endl
     << "interleaved       : "
     << (FormatIsInterleaved(format.mFormatFlags) ? "yes" : "no");
  return os;
}

static OSStatus OnGetPlayoutData(void* in_ref_con,
                                 AudioUnitRenderActionFlags* flags,
                                 const AudioTimeStamp* time_stamp,
                                 UInt32 bus_number,
                                 UInt32 num_frames,
                                 AudioBufferList* io_data) {
  *flags |= kAudioUnitRenderAction_OutputIsSilence;
  return noErr;
}

static OSStatus GetInputDeviceStreamFormat(
    AudioUnit audio_unit,
    AudioStreamBasicDescription* format) {
  DCHECK(audio_unit);
  UInt32 property_size = sizeof(*format);
  // Get the audio stream data format on the input scope of the input element
  // since it is connected to the current input device.
  OSStatus result =
      AudioUnitGetProperty(audio_unit, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Input, 1, format, &property_size);
  DVLOG(1) << "Input device stream format: " << *format;
  return result;
}

// Returns the number of physical processors on the device.
static int NumberOfPhysicalProcessors() {
  base::mac::ScopedMachSendRight mach_host(mach_host_self());
  host_basic_info hbi = {};
  mach_msg_type_number_t info_count = HOST_BASIC_INFO_COUNT;
  kern_return_t kr =
      host_info(mach_host.get(), HOST_BASIC_INFO,
                reinterpret_cast<host_info_t>(&hbi), &info_count);

  int n_physical_cores = 0;
  if (kr != KERN_SUCCESS) {
    n_physical_cores = 1;
    LOG(ERROR) << "Failed to determine number of physical cores, assuming 1";
  } else {
    n_physical_cores = hbi.physical_cpu;
  }
  DCHECK_EQ(HOST_BASIC_INFO_COUNT, info_count);
  return n_physical_cores;
}

// Adds extra system information to Media.AudioXXXMac UMA statistics.
// Only called when it has been detected that audio callbacks does not start
// as expected.
static void AddSystemInfoToUMA() {
  // Number of logical processors/cores on the current machine.
  UMA_HISTOGRAM_COUNTS_1M("Media.Audio.LogicalProcessorsMac",
                          base::SysInfo::NumberOfProcessors());
  // Number of physical processors/cores on the current machine.
  UMA_HISTOGRAM_COUNTS_1M("Media.Audio.PhysicalProcessorsMac",
                          NumberOfPhysicalProcessors());
  DVLOG(1) << "logical processors: " << base::SysInfo::NumberOfProcessors();
  DVLOG(1) << "physical processors: " << NumberOfPhysicalProcessors();
}

// Finds the first subdevice, in an aggregate device, with output streams.
static AudioDeviceID FindFirstOutputSubdevice(
    AudioDeviceID aggregate_device_id) {
  const AudioObjectPropertyAddress property_address = {
      kAudioAggregateDevicePropertyFullSubDeviceList,
      kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
  base::ScopedCFTypeRef<CFArrayRef> subdevices;
  UInt32 size = sizeof(subdevices);
  OSStatus result = AudioObjectGetPropertyData(
      aggregate_device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, subdevices.InitializeInto());

  if (result != noErr) {
    OSSTATUS_LOG(WARNING, result)
        << "Failed to read property "
        << kAudioAggregateDevicePropertyFullSubDeviceList << " for device "
        << aggregate_device_id;
    return kAudioObjectUnknown;
  }

  AudioDeviceID output_subdevice_id = kAudioObjectUnknown;
  DCHECK_EQ(CFGetTypeID(subdevices), CFArrayGetTypeID());
  const CFIndex count = CFArrayGetCount(subdevices);
  for (CFIndex i = 0; i != count; ++i) {
    CFStringRef value =
        base::mac::CFCast<CFStringRef>(CFArrayGetValueAtIndex(subdevices, i));
    if (value) {
      std::string uid = base::SysCFStringRefToUTF8(value);
      output_subdevice_id = AudioManagerMac::GetAudioDeviceIdByUId(false, uid);
      if (output_subdevice_id != kAudioObjectUnknown &&
          core_audio_mac::GetNumStreams(output_subdevice_id, false) > 0) {
        break;
      }
    }
  }

  return output_subdevice_id;
}

// See "Technical Note TN2091 - Device input using the HAL Output Audio Unit"
// http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
// for more details and background regarding this implementation.

AUAudioInputStream::AUAudioInputStream(
    AudioManagerMac* manager,
    const AudioParameters& input_params,
    AudioDeviceID audio_device_id,
    const AudioManager::LogCallback& log_callback,
    AudioManagerBase::VoiceProcessingMode voice_processing_mode)
    : manager_(manager),
      input_params_(input_params),
      number_of_frames_provided_(0),
      io_buffer_frame_size_(0),
      sink_(nullptr),
      audio_unit_(0),
      input_device_id_(audio_device_id),
      number_of_channels_in_frame_(0),
      fifo_(input_params.channels(),
            input_params.frames_per_buffer(),
            kNumberOfBlocksBufferInFifo),
      got_input_callback_(false),
      input_callback_is_active_(false),
      buffer_size_was_changed_(false),
      audio_unit_render_has_worked_(false),
      noise_reduction_suppressed_(false),
      use_voice_processing_(voice_processing_mode ==
                            AudioManagerBase::VoiceProcessingMode::kEnabled),
      output_device_id_for_aec_(kAudioObjectUnknown),
      last_sample_time_(0.0),
      last_number_of_frames_(0),
      total_lost_frames_(0),
      largest_glitch_frames_(0),
      glitches_detected_(0),
      log_callback_(log_callback) {
  DCHECK(manager_);
  CHECK(log_callback_ != AudioManager::LogCallback());
  if (use_voice_processing_) {
    DCHECK(input_params.channels() == 1 || input_params.channels() == 2);
    const bool got_default_device =
        AudioManagerMac::GetDefaultOutputDevice(&output_device_id_for_aec_);
    DCHECK(got_default_device);
  }

  const SampleFormat kSampleFormat = kSampleFormatS16;

  // Set up the desired (output) format specified by the client.
  format_.mSampleRate = input_params.sample_rate();
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags =
      kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
  DCHECK(FormatIsInterleaved(format_.mFormatFlags));
  format_.mBitsPerChannel = SampleFormatToBitsPerChannel(kSampleFormat);
  format_.mChannelsPerFrame = input_params.channels();
  format_.mFramesPerPacket = 1;  // uncompressed audio
  format_.mBytesPerPacket = format_.mBytesPerFrame =
      input_params.GetBytesPerFrame(kSampleFormat);
  format_.mReserved = 0;

  DVLOG(1) << "ctor";
  DVLOG(1) << "device ID: 0x" << std::hex << audio_device_id;
  DVLOG(1) << "buffer size : " << input_params.frames_per_buffer();
  DVLOG(1) << "channels : " << input_params.channels();
  DVLOG(1) << "desired output format: " << format_;

  // Derive size (in bytes) of the buffers that we will render to.
  UInt32 data_byte_size =
      input_params.frames_per_buffer() * format_.mBytesPerFrame;
  DVLOG(1) << "size of data buffer in bytes : " << data_byte_size;

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer.
  audio_data_buffer_.reset(new uint8_t[data_byte_size]);
  // We ask for noninterleaved audio.
  audio_buffer_list_.mNumberBuffers = 1;

  AudioBuffer* audio_buffer = audio_buffer_list_.mBuffers;
  audio_buffer->mNumberChannels = input_params.channels();
  audio_buffer->mDataByteSize = data_byte_size;
  audio_buffer->mData = audio_data_buffer_.get();
}

AUAudioInputStream::~AUAudioInputStream() {
  DVLOG(1) << "~dtor";
  ReportAndResetStats();
}

// Obtain and open the AUHAL AudioOutputUnit for recording.
bool AUAudioInputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Open";
  DCHECK(!audio_unit_);

  // Verify that we have a valid device. Send appropriate error code to
  // HandleError() to ensure that the error type is added to UMA stats.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    HandleError(kAudioUnitErr_InvalidElement);
    return false;
  }

  // The requested sample-rate must match the hardware sample-rate.
  const int sample_rate =
      AudioManagerMac::HardwareSampleRateForDevice(input_device_id_);
  DCHECK_EQ(sample_rate, format_.mSampleRate);

  log_callback_.Run(base::StrCat(
      {"AU in: Open using ", use_voice_processing_ ? "VPAU" : "AUHAL"}));

  const bool success =
      use_voice_processing_ ? OpenVoiceProcessingAU() : OpenAUHAL();

  if (!success)
    return false;

  // The hardware latency is fixed and will not change during the call.
  hardware_latency_ = AudioManagerMac::GetHardwareLatency(
      audio_unit_, input_device_id_, kAudioDevicePropertyScopeInput,
      format_.mSampleRate);

  // The master channel is 0, Left and right are channels 1 and 2.
  // And the master channel is not counted in |number_of_channels_in_frame_|.
  number_of_channels_in_frame_ = GetNumberOfChannelsFromStream();

  return true;
}

bool AUAudioInputStream::OpenAUHAL() {
  // Start by obtaining an AudioOuputUnit using an AUHAL component description.

  // Description for the Audio Unit we want to use (AUHAL in this case).
  // The kAudioUnitSubType_HALOutput audio unit interfaces to any audio device.
  // The user specifies which audio device to track. The audio unit can do
  // input from the device as well as output to the device. Bus 0 is used for
  // the output side, bus 1 is used to get audio input from the device.
  AudioComponentDescription desc = {kAudioUnitType_Output,
                                    kAudioUnitSubType_HALOutput,
                                    kAudioUnitManufacturer_Apple, 0, 0};

  // Find a component that meets the description in |desc|.
  AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
  DCHECK(comp);
  if (!comp) {
    HandleError(kAudioUnitErr_NoConnection);
    return false;
  }

  // Get access to the service provided by the specified Audio Unit.
  OSStatus result = AudioComponentInstanceNew(comp, &audio_unit_);
  if (result) {
    HandleError(result);
    return false;
  }

  // Initialize the AUHAL before making any changes or using it. The audio
  // unit will be initialized once more as last operation in this method but
  // that is intentional. This approach is based on a comment in the
  // CAPlayThrough example from Apple, which states that "AUHAL needs to be
  // initialized *before* anything is done to it".
  // TODO(henrika): remove this extra call if we are unable to see any
  // positive effects of it in our UMA stats.
  result = AudioUnitInitialize(audio_unit_);
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Enable IO on the input scope of the Audio Unit.
  // Note that, these changes must be done *before* setting the AUHAL's
  // current device.

  // After creating the AUHAL object, we must enable IO on the input scope
  // of the Audio Unit to obtain the device input. Input must be explicitly
  // enabled with the kAudioOutputUnitProperty_EnableIO property on Element 1
  // of the AUHAL. Because the AUHAL can be used for both input and output,
  // we must also disable IO on the output scope.

  // kAudioOutputUnitProperty_EnableIO is not a writable property of the
  // voice processing unit (we'd get kAudioUnitErr_PropertyNotWritable returned
  // back to us). IO is always enabled.

  // Enable input on the AUHAL.
  {
    const UInt32 enableIO = 1;
    result = AudioUnitSetProperty(
        audio_unit_, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input,
        AUElement::INPUT, &enableIO, sizeof(enableIO));
    if (result != noErr) {
      HandleError(result);
      return false;
    }
  }

  // Disable output on the AUHAL.
  {
    const UInt32 disableIO = 0;
    result = AudioUnitSetProperty(
        audio_unit_, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
        AUElement::OUTPUT, &disableIO, sizeof(disableIO));
    if (result != noErr) {
      HandleError(result);
      return false;
    }
  }

  // Next, set the audio device to be the Audio Unit's current device.
  // Note that, devices can only be set to the AUHAL after enabling IO.
  result =
      AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_CurrentDevice,
                           kAudioUnitScope_Global, AUElement::OUTPUT,
                           &input_device_id_, sizeof(input_device_id_));

  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Register the input procedure for the AUHAL. This procedure will be called
  // when the AUHAL has received new data from the input device.
  AURenderCallbackStruct callback;
  callback.inputProc = &DataIsAvailable;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      audio_unit_, kAudioOutputUnitProperty_SetInputCallback,
      kAudioUnitScope_Global, AUElement::OUTPUT, &callback, sizeof(callback));

  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Get the stream format for the selected input device and ensure that the
  // sample rate of the selected input device matches the desired (given at
  // construction) sample rate. We should not rely on sample rate conversion
  // in the AUHAL, only *simple* conversions, e.g., 32-bit float to 16-bit
  // signed integer format.
  AudioStreamBasicDescription input_device_format = {0};
  GetInputDeviceStreamFormat(audio_unit_, &input_device_format);
  if (input_device_format.mSampleRate != format_.mSampleRate) {
    LOG(ERROR)
        << "Input device's sample rate does not match the client's sample rate";
    result = kAudioUnitErr_FormatNotSupported;
    HandleError(result);
    return false;
  }

  // Modify the IO buffer size if not already set correctly for the selected
  // device. The status of other active audio input and output streams is
  // involved in the final setting.
  // TODO(henrika): we could make io_buffer_frame_size a member and add it to
  // the UMA stats tied to the Media.Audio.InputStartupSuccessMac record.
  size_t io_buffer_frame_size = 0;
  if (!manager_->MaybeChangeBufferSize(
          input_device_id_, audio_unit_, 1, input_params_.frames_per_buffer(),
          &buffer_size_was_changed_, &io_buffer_frame_size)) {
    result = kAudioUnitErr_FormatNotSupported;
    HandleError(result);
    return false;
  }

  // Store current I/O buffer frame size for UMA stats stored in combination
  // with failing input callbacks.
  DCHECK(!io_buffer_frame_size_);
  io_buffer_frame_size_ = io_buffer_frame_size;

  // If the requested number of frames is out of range, the closest valid buffer
  // size will be set instead. Check the current setting and log a warning for a
  // non perfect match. Any such mismatch will be compensated for in
  // OnDataIsAvailable().
  UInt32 buffer_frame_size = 0;
  UInt32 property_size = sizeof(buffer_frame_size);
  result = AudioUnitGetProperty(
      audio_unit_, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global,
      AUElement::OUTPUT, &buffer_frame_size, &property_size);
  LOG_IF(WARNING, buffer_frame_size !=
                      static_cast<UInt32>(input_params_.frames_per_buffer()))
      << "AUHAL is using best match of IO buffer size: " << buffer_frame_size;

  // Channel mapping should be supported but add a warning just in case.
  // TODO(henrika): perhaps add to UMA stat to track if this can happen.
  DLOG_IF(WARNING,
          input_device_format.mChannelsPerFrame != format_.mChannelsPerFrame)
      << "AUHAL's audio converter must do channel conversion";

  // Set up the the desired (output) format.
  // For obtaining input from a device, the device format is always expressed
  // on the output scope of the AUHAL's Element 1.
  result = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output, 1, &format_,
                                sizeof(format_));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Finally, initialize the audio unit and ensure that it is ready to render.
  // Allocates memory according to the maximum number of audio frames
  // it can produce in response to a single render call.
  result = AudioUnitInitialize(audio_unit_);
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  return true;
}

bool AUAudioInputStream::OpenVoiceProcessingAU() {
  // Start by obtaining an AudioOuputUnit using an AUHAL component description.

  // Description for the Audio Unit we want to use (AUHAL in this case).
  // The kAudioUnitSubType_HALOutput audio unit interfaces to any audio device.
  // The user specifies which audio device to track. The audio unit can do
  // input from the device as well as output to the device. Bus 0 is used for
  // the output side, bus 1 is used to get audio input from the device.
  AudioComponentDescription desc = {kAudioUnitType_Output,
                                    kAudioUnitSubType_VoiceProcessingIO,
                                    kAudioUnitManufacturer_Apple, 0, 0};

  // Find a component that meets the description in |desc|.
  AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
  DCHECK(comp);
  if (!comp) {
    HandleError(kAudioUnitErr_NoConnection);
    return false;
  }

  // Get access to the service provided by the specified Audio Unit.
  OSStatus result = AudioComponentInstanceNew(comp, &audio_unit_);
  if (result) {
    HandleError(result);
    return false;
  }

  // Next, set the audio device to be the Audio Unit's input device.
  result =
      AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_CurrentDevice,
                           kAudioUnitScope_Global, AUElement::INPUT,
                           &input_device_id_, sizeof(input_device_id_));

  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Followed by the audio device to be the Audio Unit's output device.
  result = AudioUnitSetProperty(
      audio_unit_, kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global, AUElement::OUTPUT, &output_device_id_for_aec_,
      sizeof(output_device_id_for_aec_));

  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Register the input procedure for the AUHAL. This procedure will be called
  // when the AUHAL has received new data from the input device.
  AURenderCallbackStruct callback;
  callback.inputProc = &DataIsAvailable;
  callback.inputProcRefCon = this;

  result = AudioUnitSetProperty(
      audio_unit_, kAudioOutputUnitProperty_SetInputCallback,
      kAudioUnitScope_Global, AUElement::INPUT, &callback, sizeof(callback));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  callback.inputProc = OnGetPlayoutData;
  callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      audio_unit_, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
      AUElement::OUTPUT, &callback, sizeof(callback));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Get the stream format for the selected input device and ensure that the
  // sample rate of the selected input device matches the desired (given at
  // construction) sample rate. We should not rely on sample rate conversion
  // in the AUHAL, only *simple* conversions, e.g., 32-bit float to 16-bit
  // signed integer format.
  AudioStreamBasicDescription input_device_format = {0};
  GetInputDeviceStreamFormat(audio_unit_, &input_device_format);
  if (input_device_format.mSampleRate != format_.mSampleRate) {
    LOG(ERROR)
        << "Input device's sample rate does not match the client's sample rate";
    result = kAudioUnitErr_FormatNotSupported;
    HandleError(result);
    return false;
  }

  // Modify the IO buffer size if not already set correctly for the selected
  // device. The status of other active audio input and output streams is
  // involved in the final setting.
  // TODO(henrika): we could make io_buffer_frame_size a member and add it to
  // the UMA stats tied to the Media.Audio.InputStartupSuccessMac record.
  size_t io_buffer_frame_size = 0;
  if (!manager_->MaybeChangeBufferSize(
          input_device_id_, audio_unit_, 1, input_params_.frames_per_buffer(),
          &buffer_size_was_changed_, &io_buffer_frame_size)) {
    result = kAudioUnitErr_FormatNotSupported;
    HandleError(result);
    return false;
  }

  // Store current I/O buffer frame size for UMA stats stored in combination
  // with failing input callbacks.
  DCHECK(!io_buffer_frame_size_);
  io_buffer_frame_size_ = io_buffer_frame_size;

  // If the requested number of frames is out of range, the closest valid buffer
  // size will be set instead. Check the current setting and log a warning for a
  // non perfect match. Any such mismatch will be compensated for in
  // OnDataIsAvailable().
  UInt32 buffer_frame_size = 0;
  UInt32 property_size = sizeof(buffer_frame_size);
  result = AudioUnitGetProperty(
      audio_unit_, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global,
      AUElement::OUTPUT, &buffer_frame_size, &property_size);
  LOG_IF(WARNING, buffer_frame_size !=
                      static_cast<UInt32>(input_params_.frames_per_buffer()))
      << "AUHAL is using best match of IO buffer size: " << buffer_frame_size;

  // The built-in device claims to be stereo. VPAU claims 5 channels (for me)
  // but refuses to work in stereo. Just accept stero for now, use mono
  // internally and upmix.
  AudioStreamBasicDescription mono_format = format_;
  if (format_.mChannelsPerFrame == 2) {
    mono_format.mChannelsPerFrame = 1;
    mono_format.mBytesPerPacket = mono_format.mBitsPerChannel / 8;
    mono_format.mBytesPerFrame = mono_format.mBytesPerPacket;
  }

  // Set up the the desired (output) format.
  // For obtaining input from a device, the device format is always expressed
  // on the output scope of the AUHAL's Element 1.
  result = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output, AUElement::INPUT,
                                &mono_format, sizeof(mono_format));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  result = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input, AUElement::OUTPUT,
                                &mono_format, sizeof(mono_format));
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  // Finally, initialize the audio unit and ensure that it is ready to render.
  // Allocates memory according to the maximum number of audio frames
  // it can produce in response to a single render call.
  result = AudioUnitInitialize(audio_unit_);
  if (result != noErr) {
    HandleError(result);
    return false;
  }

  UndoDucking(output_device_id_for_aec_);

  return true;
}

void AUAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Start";
  DCHECK(callback);
  DCHECK(!sink_);
  DLOG_IF(ERROR, !audio_unit_) << "Open() has not been called successfully";
  if (IsRunning())
    return;

  // Check if we should defer Start() for http://crbug.com/160920.
  if (manager_->ShouldDeferStreamStart()) {
    LOG(WARNING) << "Start of input audio is deferred";
    // Use a cancellable closure so that if Stop() is called before Start()
    // actually runs, we can cancel the pending start.
    deferred_start_cb_.Reset(base::Bind(&AUAudioInputStream::Start,
                                        base::Unretained(this), callback));
    manager_->GetTaskRunner()->PostDelayedTask(
        FROM_HERE, deferred_start_cb_.callback(),
        base::TimeDelta::FromSeconds(
            AudioManagerMac::kStartDelayInSecsForPowerEvents));
    return;
  }

  sink_ = callback;
  last_success_time_ = base::TimeTicks::Now();
  audio_unit_render_has_worked_ = false;

  // Don't disable built-in noise suppression when using VPAU.
  if (!use_voice_processing_ &&
      !(input_params_.effects() & AudioParameters::NOISE_SUPPRESSION) &&
      manager_->DeviceSupportsAmbientNoiseReduction(input_device_id_)) {
    noise_reduction_suppressed_ =
        manager_->SuppressNoiseReduction(input_device_id_);
  }
  StartAgc();
  OSStatus result = AudioOutputUnitStart(audio_unit_);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "Failed to start acquiring data";
  if (result != noErr) {
    Stop();
    return;
  }
  DCHECK(IsRunning()) << "Audio unit started OK but is not yet running";

  // For UMA stat purposes, start a one-shot timer which detects when input
  // callbacks starts indicating if input audio recording starts as intended.
  // CheckInputStartupSuccess() will check if |input_callback_is_active_| is
  // true when the timer expires.
  input_callback_timer_.reset(new base::OneShotTimer());
  input_callback_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kInputCallbackStartTimeoutInSeconds), this,
      &AUAudioInputStream::CheckInputStartupSuccess);
  DCHECK(input_callback_timer_->IsRunning());
}

void AUAudioInputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  deferred_start_cb_.Cancel();
  DVLOG(1) << "Stop";
  StopAgc();
  if (noise_reduction_suppressed_) {
    manager_->UnsuppressNoiseReduction(input_device_id_);
    noise_reduction_suppressed_ = false;
  }
  if (input_callback_timer_ != nullptr) {
    input_callback_timer_->Stop();
    input_callback_timer_.reset();
  }

  if (audio_unit_ != nullptr) {
    // Stop the I/O audio unit.
    OSStatus result = AudioOutputUnitStop(audio_unit_);
    DCHECK_EQ(result, noErr);
    // Add a DCHECK here just in case. AFAIK, the call to AudioOutputUnitStop()
    // seems to set this state synchronously, hence it should always report
    // false after a successful call.
    DCHECK(!IsRunning()) << "Audio unit is stopped but still running";

    // Reset the audio unit’s render state. This function clears memory.
    // It does not allocate or free memory resources.
    result = AudioUnitReset(audio_unit_, kAudioUnitScope_Global, 0);
    DCHECK_EQ(result, noErr);
    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "Failed to stop acquiring data";
  }

  SetInputCallbackIsActive(false);
  ReportAndResetStats();
  sink_ = nullptr;
  fifo_.Clear();
  io_buffer_frame_size_ = 0;
  got_input_callback_ = false;
}

void AUAudioInputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Close";

  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  if (IsRunning()) {
    Stop();
  }

  // Uninitialize and dispose the audio unit.
  CloseAudioUnit();

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

double AUAudioInputStream::GetMaxVolume() {
  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return 0.0;
  }

  // Query if any of the master, left or right channels has volume control.
  for (int i = 0; i <= number_of_channels_in_frame_; ++i) {
    // If the volume is settable, the  valid volume range is [0.0, 1.0].
    if (IsVolumeSettableOnChannel(i))
      return 1.0;
  }

  // Volume control is not available for the audio stream.
  return 0.0;
}

void AUAudioInputStream::SetVolume(double volume) {
  DVLOG(1) << "SetVolume(volume=" << volume << ")";
  DCHECK_GE(volume, 0.0);
  DCHECK_LE(volume, 1.0);

  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return;
  }

  Float32 volume_float32 = static_cast<Float32>(volume);
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMaster};

  // Try to set the volume for master volume channel.
  if (IsVolumeSettableOnChannel(kAudioObjectPropertyElementMaster)) {
    OSStatus result = AudioObjectSetPropertyData(
        input_device_id_, &property_address, 0, nullptr, sizeof(volume_float32),
        &volume_float32);
    if (result != noErr) {
      DLOG(WARNING) << "Failed to set volume to " << volume_float32;
    }
    return;
  }

  // There is no master volume control, try to set volume for each channel.
  int successful_channels = 0;
  for (int i = 1; i <= number_of_channels_in_frame_; ++i) {
    property_address.mElement = static_cast<UInt32>(i);
    if (IsVolumeSettableOnChannel(i)) {
      OSStatus result = AudioObjectSetPropertyData(
          input_device_id_, &property_address, 0, NULL, sizeof(volume_float32),
          &volume_float32);
      if (result == noErr)
        ++successful_channels;
    }
  }

  DLOG_IF(WARNING, successful_channels == 0)
      << "Failed to set volume to " << volume_float32;

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double AUAudioInputStream::GetVolume() {
  // Verify that we have a valid device.
  if (input_device_id_ == kAudioObjectUnknown) {
    NOTREACHED() << "Device ID is unknown";
    return 0.0;
  }

  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMaster};

  if (AudioObjectHasProperty(input_device_id_, &property_address)) {
    // The device supports master volume control, get the volume from the
    // master channel.
    Float32 volume_float32 = 0.0;
    UInt32 size = sizeof(volume_float32);
    OSStatus result =
        AudioObjectGetPropertyData(input_device_id_, &property_address, 0,
                                   nullptr, &size, &volume_float32);
    if (result == noErr)
      return static_cast<double>(volume_float32);
  } else {
    // There is no master volume control, try to get the average volume of
    // all the channels.
    Float32 volume_float32 = 0.0;
    int successful_channels = 0;
    for (int i = 1; i <= number_of_channels_in_frame_; ++i) {
      property_address.mElement = static_cast<UInt32>(i);
      if (AudioObjectHasProperty(input_device_id_, &property_address)) {
        Float32 channel_volume = 0;
        UInt32 size = sizeof(channel_volume);
        OSStatus result =
            AudioObjectGetPropertyData(input_device_id_, &property_address, 0,
                                       nullptr, &size, &channel_volume);
        if (result == noErr) {
          volume_float32 += channel_volume;
          ++successful_channels;
        }
      }
    }

    // Get the average volume of the channels.
    if (successful_channels != 0)
      return static_cast<double>(volume_float32 / successful_channels);
  }

  DLOG(WARNING) << "Failed to get volume";
  return 0.0;
}

bool AUAudioInputStream::IsMuted() {
  // Verify that we have a valid device.
  DCHECK_NE(input_device_id_, kAudioObjectUnknown) << "Device ID is unknown";

  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMaster};

  if (!AudioObjectHasProperty(input_device_id_, &property_address)) {
    DLOG(ERROR) << "Device does not support checking master mute state";
    return false;
  }

  UInt32 muted = 0;
  UInt32 size = sizeof(muted);
  OSStatus result = AudioObjectGetPropertyData(
      input_device_id_, &property_address, 0, nullptr, &size, &muted);
  DLOG_IF(WARNING, result != noErr) << "Failed to get mute state";
  return result == noErr && muted != 0;
}

void AUAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  if (!use_voice_processing_)
    return;

  AudioDeviceID audio_device_id =
      AudioManagerMac::GetAudioDeviceIdByUId(false, output_device_id);
  if (audio_device_id == output_device_id_for_aec_)
    return;

  if (audio_device_id == kAudioObjectUnknown) {
    log_callback_.Run(
        base::StringPrintf("AU in: Unable to resolve output device id '%s'",
                           output_device_id.c_str()));
    return;
  }

  // If the selected device is an aggregate device, try to use the first output
  // device of the aggregate device instead.
  if (core_audio_mac::GetDeviceTransportType(audio_device_id) ==
      kAudioDeviceTransportTypeAggregate) {
    const AudioDeviceID output_subdevice_id =
        FindFirstOutputSubdevice(audio_device_id);

    if (output_subdevice_id == kAudioObjectUnknown) {
      log_callback_.Run(base::StringPrintf(
          "AU in: Unable to find an output subdevice in aggregate device '%s'",
          output_device_id.c_str()));
      return;
    }
    audio_device_id = output_subdevice_id;
  }

  if (audio_device_id != output_device_id_for_aec_) {
    output_device_id_for_aec_ = audio_device_id;
    log_callback_.Run(base::StringPrintf(
        "AU in: Output device for AEC changed to '%s' (%d)",
        output_device_id.c_str(), output_device_id_for_aec_));
    // Only restart the stream if it has previously been started.
    if (audio_unit_)
      ReinitializeVoiceProcessingAudioUnit();
  }
}

void AUAudioInputStream::ReinitializeVoiceProcessingAudioUnit() {
  DCHECK(use_voice_processing_);
  DCHECK(audio_unit_);

  const bool was_running = IsRunning();
  OSStatus result = noErr;

  if (was_running) {
    result = AudioOutputUnitStop(audio_unit_);
    DCHECK_EQ(result, noErr);
  }

  CloseAudioUnit();

  // Reset things to a state similar to before the audio unit was opened.
  // Most of these will be no-ops if the audio unit was opened but not started.
  SetInputCallbackIsActive(false);
  ReportAndResetStats();
  io_buffer_frame_size_ = 0;
  got_input_callback_ = false;

  OpenVoiceProcessingAU();

  if (was_running) {
    result = AudioOutputUnitStart(audio_unit_);
    if (result != noErr) {
      OSSTATUS_DLOG(ERROR, result) << "Failed to start acquiring data";
      Stop();
      return;
    }
  }

  log_callback_.Run(base::StringPrintf(
      "AU in: Successfully reinitialized AEC for output device id=%d.",
      output_device_id_for_aec_));
}

// static
OSStatus AUAudioInputStream::DataIsAvailable(void* context,
                                             AudioUnitRenderActionFlags* flags,
                                             const AudioTimeStamp* time_stamp,
                                             UInt32 bus_number,
                                             UInt32 number_of_frames,
                                             AudioBufferList* io_data) {
  DCHECK(context);
  // Recorded audio is always on the input bus (=1).
  DCHECK_EQ(bus_number, 1u);
  // No data buffer should be allocated at this stage.
  DCHECK(!io_data);
  AUAudioInputStream* self = reinterpret_cast<AUAudioInputStream*>(context);
  // Propagate render action flags, time stamp, bus number and number
  // of frames requested to the AudioUnitRender() call where the actual data
  // is received from the input device via the output scope of the audio unit.
  return self->OnDataIsAvailable(flags, time_stamp, bus_number,
                                 number_of_frames);
}

OSStatus AUAudioInputStream::OnDataIsAvailable(
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 number_of_frames) {
  TRACE_EVENT0("audio", "AUAudioInputStream::OnDataIsAvailable");

  // Indicate that input callbacks have started.
  if (!got_input_callback_) {
    got_input_callback_ = true;
    SetInputCallbackIsActive(true);
  }

  // Update the |mDataByteSize| value in the audio_buffer_list() since
  // |number_of_frames| can be changed on the fly.
  // |mDataByteSize| needs to be exactly mapping to |number_of_frames|,
  // otherwise it will put CoreAudio into bad state and results in
  // AudioUnitRender() returning -50 for the new created stream.
  // We have also seen kAudioUnitErr_TooManyFramesToProcess (-10874) and
  // kAudioUnitErr_CannotDoInCurrentContext (-10863) as error codes.
  // See crbug/428706 for details.
  UInt32 new_size = number_of_frames * format_.mBytesPerFrame;
  AudioBuffer* audio_buffer = audio_buffer_list_.mBuffers;
  bool new_buffer_size_detected = false;
  if (new_size != audio_buffer->mDataByteSize) {
    new_buffer_size_detected = true;
    DVLOG(1) << "New size of number_of_frames detected: " << number_of_frames;
    io_buffer_frame_size_ = static_cast<size_t>(number_of_frames);
    if (new_size > audio_buffer->mDataByteSize) {
      // This can happen if the device is unplugged during recording. We
      // allocate enough memory here to avoid depending on how CoreAudio
      // handles it.
      // See See http://www.crbug.com/434681 for one example when we can enter
      // this scope.
      audio_data_buffer_.reset(new uint8_t[new_size]);
      audio_buffer->mData = audio_data_buffer_.get();
    }

    // Update the |mDataByteSize| to match |number_of_frames|.
    audio_buffer->mDataByteSize = new_size;
  }

  // Obtain the recorded audio samples by initiating a rendering cycle.
  // Since it happens on the input bus, the |&audio_buffer_list_| parameter is
  // a reference to the preallocated audio buffer list that the audio unit
  // renders into.
  OSStatus result;
  if (use_voice_processing_ && format_.mChannelsPerFrame != 1) {
    // Use the first part of the output buffer for mono data...
    AudioBufferList mono_buffer_list;
    mono_buffer_list.mNumberBuffers = 1;
    AudioBuffer* mono_buffer = mono_buffer_list.mBuffers;
    mono_buffer->mNumberChannels = 1;
    mono_buffer->mDataByteSize =
        audio_buffer->mDataByteSize / audio_buffer->mNumberChannels;
    mono_buffer->mData = audio_buffer->mData;

    TRACE_EVENT_BEGIN0("audio", "AudioUnitRender");
    result = AudioUnitRender(audio_unit_, flags, time_stamp, bus_number,
                             number_of_frames, &mono_buffer_list);
    TRACE_EVENT_END0("audio", "AudioUnitRender");
    // ... then upmix it by copying it out to two channels.
    UpmixMonoToStereoInPlace(audio_buffer, format_.mBitsPerChannel / 8);
  } else {
    TRACE_EVENT_BEGIN0("audio", "AudioUnitRender");
    result = AudioUnitRender(audio_unit_, flags, time_stamp, bus_number,
                             number_of_frames, &audio_buffer_list_);
    TRACE_EVENT_END0("audio", "AudioUnitRender");
  }

  if (result == noErr) {
    audio_unit_render_has_worked_ = true;
  }
  if (result) {
    TRACE_EVENT_INSTANT0("audio", "AudioUnitRender error",
                         TRACE_EVENT_SCOPE_THREAD);
    // Only upload UMA histograms for the case when AGC is enabled. The reason
    // is that we want to compare these stats with others in this class and
    // they are only stored for "AGC streams", e.g. WebRTC audio streams.
    const bool add_uma_histogram = GetAutomaticGainControl();
    if (add_uma_histogram) {
      base::UmaHistogramSparse("Media.AudioInputCbErrorMac", result);
    }
    OSSTATUS_LOG(ERROR, result) << "AudioUnitRender() failed ";
    if (result == kAudioUnitErr_TooManyFramesToProcess ||
        result == kAudioUnitErr_CannotDoInCurrentContext) {
      DCHECK(!last_success_time_.is_null());
      // We delay stopping the stream for kAudioUnitErr_TooManyFramesToProcess
      // since it has been observed that some USB headsets can cause this error
      // but only for a few initial frames at startup and then then the stream
      // returns to a stable state again. See b/19524368 for details.
      // Instead, we measure time since last valid audio frame and call
      // HandleError() only if a too long error sequence is detected. We do
      // this to avoid ending up in a non recoverable bad core audio state.
      // Also including kAudioUnitErr_CannotDoInCurrentContext since long
      // sequences can be produced in combination with e.g. sample-rate changes
      // for input devices.
      base::TimeDelta time_since_last_success =
          base::TimeTicks::Now() - last_success_time_;
      if ((time_since_last_success >
           base::TimeDelta::FromSeconds(kMaxErrorTimeoutInSeconds))) {
        const char* err = (result == kAudioUnitErr_TooManyFramesToProcess)
                              ? "kAudioUnitErr_TooManyFramesToProcess"
                              : "kAudioUnitErr_CannotDoInCurrentContext";
        LOG(ERROR) << "Too long sequence of " << err << " errors!";
        HandleError(result);
      }

      // Add some extra UMA stat to track down if we see this particular error
      // in combination with a previous change of buffer size "on the fly".
      if (result == kAudioUnitErr_CannotDoInCurrentContext &&
          add_uma_histogram) {
        UMA_HISTOGRAM_BOOLEAN("Media.Audio.RenderFailsWhenBufferSizeChangesMac",
                              new_buffer_size_detected);
        UMA_HISTOGRAM_BOOLEAN("Media.Audio.AudioUnitRenderHasWorkedMac",
                              audio_unit_render_has_worked_);
      }
    } else {
      // We have also seen kAudioUnitErr_NoConnection in some cases. Bailing
      // out for this error for now.
      HandleError(result);
    }
    return result;
  }
  // Update time of successful call to AudioUnitRender().
  last_success_time_ = base::TimeTicks::Now();

  // Deliver recorded data to the consumer as a callback.
  return Provide(number_of_frames, &audio_buffer_list_, time_stamp);
}

OSStatus AUAudioInputStream::Provide(UInt32 number_of_frames,
                                     AudioBufferList* io_data,
                                     const AudioTimeStamp* time_stamp) {
  TRACE_EVENT1("audio", "AUAudioInputStream::Provide", "number_of_frames",
               number_of_frames);
  UpdateCaptureTimestamp(time_stamp);
  last_number_of_frames_ = number_of_frames;

  // TODO(grunell): We'll only care about the first buffer size change, any
  // further changes will be ignored. This is in line with output side stats.
  // It would be nice to have all changes reflected in UMA stats.
  if (number_of_frames !=
          static_cast<UInt32>(input_params_.frames_per_buffer()) &&
      number_of_frames_provided_ == 0)
    number_of_frames_provided_ = number_of_frames;

  base::TimeTicks capture_time = GetCaptureTime(time_stamp);

  // The AGC volume level is updated once every second on a separate thread.
  // Note that, |volume| is also updated each time SetVolume() is called
  // through IPC by the render-side AGC.
  double normalized_volume = 0.0;
  GetAgcVolume(&normalized_volume);

  AudioBuffer& buffer = io_data->mBuffers[0];
  uint8_t* audio_data = reinterpret_cast<uint8_t*>(buffer.mData);
  DCHECK(audio_data);
  if (!audio_data)
    return kAudioUnitErr_InvalidElement;

  // Dynamically increase capacity of the FIFO to handle larger buffers from
  // CoreAudio. This can happen in combination with Apple Thunderbolt Displays
  // when the Display Audio is used as capture source and the cable is first
  // remove and then inserted again.
  // See http://www.crbug.com/434681 for details.
  if (static_cast<int>(number_of_frames) > fifo_.GetUnfilledFrames()) {
    // Derive required increase in number of FIFO blocks. The increase is
    // typically one block.
    const int blocks =
        static_cast<int>((number_of_frames - fifo_.GetUnfilledFrames()) /
                         input_params_.frames_per_buffer()) +
        1;
    DLOG(WARNING) << "Increasing FIFO capacity by " << blocks << " blocks";
    TRACE_EVENT_INSTANT1("audio", "Increasing FIFO capacity",
                         TRACE_EVENT_SCOPE_THREAD, "increased by", blocks);
    fifo_.IncreaseCapacity(blocks);
  }

  // Compensate the capture time for the FIFO before pushing an new frames.
  capture_time -= AudioTimestampHelper::FramesToTime(fifo_.GetAvailableFrames(),
                                                     format_.mSampleRate);

  // Copy captured (and interleaved) data into FIFO.
  fifo_.Push(audio_data, number_of_frames, format_.mBitsPerChannel / 8);

  // Consume and deliver the data when the FIFO has a block of available data.
  while (fifo_.available_blocks()) {
    const AudioBus* audio_bus = fifo_.Consume();
    DCHECK_EQ(audio_bus->frames(),
              static_cast<int>(input_params_.frames_per_buffer()));

    sink_->OnData(audio_bus, capture_time, normalized_volume);

    // Move the capture time forward for each vended block.
    capture_time += AudioTimestampHelper::FramesToTime(audio_bus->frames(),
                                                       format_.mSampleRate);
  }

  return noErr;
}

int AUAudioInputStream::HardwareSampleRate() {
  // Determine the default input device's sample-rate.
  AudioDeviceID device_id = kAudioObjectUnknown;
  UInt32 info_size = sizeof(device_id);

  AudioObjectPropertyAddress default_input_device_address = {
      kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster};
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &default_input_device_address, 0,
                                               0, &info_size, &device_id);
  if (result != noErr)
    return 0.0;

  Float64 nominal_sample_rate;
  info_size = sizeof(nominal_sample_rate);

  AudioObjectPropertyAddress nominal_sample_rate_address = {
      kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster};
  result = AudioObjectGetPropertyData(device_id, &nominal_sample_rate_address,
                                      0, 0, &info_size, &nominal_sample_rate);
  if (result != noErr)
    return 0.0;

  return static_cast<int>(nominal_sample_rate);
}

base::TimeTicks AUAudioInputStream::GetCaptureTime(
    const AudioTimeStamp* input_time_stamp) {
  // Total latency is composed by the dynamic latency and the fixed
  // hardware latency.
  // https://lists.apple.com/archives/coreaudio-api/2017/Jul/msg00035.html
  return (input_time_stamp->mFlags & kAudioTimeStampHostTimeValid
              ? base::TimeTicks::FromMachAbsoluteTime(
                    input_time_stamp->mHostTime)
              : base::TimeTicks::Now()) -
         hardware_latency_;
}

int AUAudioInputStream::GetNumberOfChannelsFromStream() {
  // Get the stream format, to be able to read the number of channels.
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMaster};
  AudioStreamBasicDescription stream_format;
  UInt32 size = sizeof(stream_format);
  OSStatus result = AudioObjectGetPropertyData(
      input_device_id_, &property_address, 0, nullptr, &size, &stream_format);
  if (result != noErr) {
    DLOG(WARNING) << "Could not get stream format";
    return 0;
  }

  return static_cast<int>(stream_format.mChannelsPerFrame);
}

bool AUAudioInputStream::IsRunning() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!audio_unit_)
    return false;
  UInt32 is_running = 0;
  UInt32 size = sizeof(is_running);
  OSStatus error =
      AudioUnitGetProperty(audio_unit_, kAudioOutputUnitProperty_IsRunning,
                           kAudioUnitScope_Global, 0, &is_running, &size);
  OSSTATUS_DLOG_IF(ERROR, error != noErr, error)
      << "AudioUnitGetProperty(kAudioOutputUnitProperty_IsRunning) failed";
  DVLOG(1) << "IsRunning: " << is_running;
  return (error == noErr && is_running);
}

void AUAudioInputStream::HandleError(OSStatus err) {
  // Log the latest OSStatus error message and also change the sign of the
  // error if no callbacks are active. I.e., the sign of the error message
  // carries one extra level of information.
  base::UmaHistogramSparse("Media.InputErrorMac",
                           GetInputCallbackIsActive() ? err : (err * -1));
  NOTREACHED() << "error " << logging::DescriptionFromOSStatus(err) << " ("
               << err << ")";
  if (sink_)
    sink_->OnError();
}

bool AUAudioInputStream::IsVolumeSettableOnChannel(int channel) {
  Boolean is_settable = false;
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput,
      static_cast<UInt32>(channel)};
  OSStatus result = AudioObjectIsPropertySettable(
      input_device_id_, &property_address, &is_settable);
  return (result == noErr) ? is_settable : false;
}

void AUAudioInputStream::SetInputCallbackIsActive(bool enabled) {
  base::subtle::Release_Store(&input_callback_is_active_, enabled);
}

bool AUAudioInputStream::GetInputCallbackIsActive() {
  return (base::subtle::Acquire_Load(&input_callback_is_active_) != false);
}

void AUAudioInputStream::CheckInputStartupSuccess() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsRunning());
  // Only add UMA stat related to failing input audio for streams where
  // the AGC has been enabled, e.g. WebRTC audio input streams.
  if (GetAutomaticGainControl()) {
    // Check if we have called Start() and input callbacks have actually
    // started in time as they should. If that is not the case, we have a
    // problem and the stream is considered dead.
    const bool input_callback_is_active = GetInputCallbackIsActive();
    UMA_HISTOGRAM_BOOLEAN("Media.Audio.InputStartupSuccessMac",
                          input_callback_is_active);
    DVLOG(1) << "input_callback_is_active: " << input_callback_is_active;
    if (!input_callback_is_active) {
      // Now when we know that startup has failed for some reason, add extra
      // UMA stats in an attempt to figure out the exact reason.
      AddHistogramsForFailedStartup();
    }
  }
}

void AUAudioInputStream::CloseAudioUnit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "CloseAudioUnit";
  if (!audio_unit_)
    return;
  OSStatus result = AudioUnitUninitialize(audio_unit_);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioUnitUninitialize() failed.";
  result = AudioComponentInstanceDispose(audio_unit_);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioComponentInstanceDispose() failed.";
  audio_unit_ = 0;
}

void AUAudioInputStream::AddHistogramsForFailedStartup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_BOOLEAN("Media.Audio.InputBufferSizeWasChangedMac",
                        buffer_size_was_changed_);
  // |input_params_.frames_per_buffer()| is set at construction and corresponds
  // to the requested (by the client) number of audio frames per I/O buffer
  // connected to the selected input device. Ideally, this size will be the same
  // as the native I/O buffer size given by |io_buffer_frame_size_|.
  base::UmaHistogramSparse("Media.Audio.RequestedInputBufferFrameSizeMac",
                           input_params_.frames_per_buffer());
  DVLOG(1) << "number_of_frames_: " << input_params_.frames_per_buffer();
  // This value indicates the number of frames in the IO buffers connected to
  // the selected input device. It has been set by the audio manger in Open()
  // and can be the same as |input_params_.frames_per_buffer()|, which is the
  // desired buffer size. These two values might differ if other streams are
  // using the same device and any of these streams have asked for a smaller
  // buffer size.
  base::UmaHistogramSparse("Media.Audio.ActualInputBufferFrameSizeMac",
                           io_buffer_frame_size_);
  DVLOG(1) << "io_buffer_frame_size_: " << io_buffer_frame_size_;
  // Add information about things like number of logical processors etc.
  AddSystemInfoToUMA();
}

void AUAudioInputStream::UpdateCaptureTimestamp(
    const AudioTimeStamp* timestamp) {
  if ((timestamp->mFlags & kAudioTimeStampSampleTimeValid) == 0)
    return;

  if (last_sample_time_) {
    DCHECK_NE(0U, last_number_of_frames_);
    UInt32 diff =
        static_cast<UInt32>(timestamp->mSampleTime - last_sample_time_);
    if (diff != last_number_of_frames_) {
      DCHECK_GT(diff, last_number_of_frames_);
      // We were given samples post what we expected. Update the glitch count
      // etc. and keep a record of the largest glitch.
      auto lost_frames = diff - last_number_of_frames_;
      total_lost_frames_ += lost_frames;
      if (lost_frames > largest_glitch_frames_)
        largest_glitch_frames_ = lost_frames;
      ++glitches_detected_;
    }
  }

  // Store the last sample time for use next time we get called back.
  last_sample_time_ = timestamp->mSampleTime;
}

void AUAudioInputStream::ReportAndResetStats() {
  if (last_sample_time_ == 0)
    return;  // No stats gathered to report.

  // A value of 0 indicates that we got the buffer size we asked for.
  UMA_HISTOGRAM_COUNTS_10000("Media.Audio.Capture.FramesProvided",
                             number_of_frames_provided_);
  // Even if there aren't any glitches, we want to record it to get a feel for
  // how often we get no glitches vs the alternative.
  UMA_HISTOGRAM_COUNTS_1M("Media.Audio.Capture.Glitches", glitches_detected_);

  auto lost_frames_ms = (total_lost_frames_ * 1000) / format_.mSampleRate;
  std::string log_message = base::StringPrintf(
      "AU in: Total glitches=%d. Total frames lost=%d (%.0lf ms).",
      glitches_detected_, total_lost_frames_, lost_frames_ms);
  log_callback_.Run(log_message);

  if (glitches_detected_ != 0) {
    UMA_HISTOGRAM_LONG_TIMES("Media.Audio.Capture.LostFramesInMs",
                             base::TimeDelta::FromMilliseconds(lost_frames_ms));
    auto largest_glitch_ms =
        (largest_glitch_frames_ * 1000) / format_.mSampleRate;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.Audio.Capture.LargestGlitchMs",
        base::TimeDelta::FromMilliseconds(largest_glitch_ms),
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
        50);
    DLOG(WARNING) << log_message;
  }

  number_of_frames_provided_ = 0;
  glitches_detected_ = 0;
  last_sample_time_ = 0;
  last_number_of_frames_ = 0;
  total_lost_frames_ = 0;
  largest_glitch_frames_ = 0;
}

// TODO(ossu): Ideally, we'd just use the mono stream directly. However, since
// mono or stereo (may) depend on if we want to run the echo canceller, and
// since we can't provide two sets of AudioParameters for a device, this is the
// best we can do right now.
//
// The algorithm works by copying a sample at offset N to 2*N and 2*N + 1, e.g.:
//  ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// | a1 | a2 | a3 | b1 | b2 | b3 | c1 | c2 | c3 | -- | -- | -- | -- | -- | ...
//  ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//  into
//  ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// | a1 | a2 | a3 | a1 | a2 | a3 | b1 | b2 | b3 | b1 | b2 | b3 | c1 | c2 | ...
//  ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// To support various different sample sizes, this is done byte-by-byte. Only
// the first half of the buffer will be used as input. It is expected to contain
// mono audio. The second half is output only. Since the data is expanding, the
// algorithm starts copying from the last sample. Otherwise it would overwrite
// data not already copied.
void AUAudioInputStream::UpmixMonoToStereoInPlace(AudioBuffer* audio_buffer,
                                                  int bytes_per_sample) {
  constexpr int channels = 2;
  DCHECK_EQ(audio_buffer->mNumberChannels, static_cast<UInt32>(channels));
  const int total_bytes = audio_buffer->mDataByteSize;
  const int frames = total_bytes / bytes_per_sample / channels;
  char* byte_ptr = reinterpret_cast<char*>(audio_buffer->mData);
  for (int i = frames - 1; i >= 0; --i) {
    int in_offset = (bytes_per_sample * i);
    int out_offset = (channels * bytes_per_sample * i);
    for (int b = 0; b < bytes_per_sample; ++b) {
      const char byte = byte_ptr[in_offset + b];
      byte_ptr[out_offset + b] = byte;
      byte_ptr[out_offset + bytes_per_sample + b] = byte;
    }
  }
}

}  // namespace media
