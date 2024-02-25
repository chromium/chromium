// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_mac.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "remoting/base/host_settings.h"
#include "remoting/base/logging.h"
#include "remoting/host/host_setting_keys.h"
#include "remoting/host/mac/permission_utils.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

namespace {

// TODO(yuweih): Determine the device's sample rate. This probably still works
// with higher device sampling rate as AudioQueue will just downsample it.
constexpr AudioPacket::SamplingRate kSampleRate =
    AudioPacket::SAMPLING_RATE_44100;
constexpr int kBytesPerChannel = 2;
constexpr int kChannelsPerFrame = 2;  // Stereo
constexpr int kBytesPerFrame = kBytesPerChannel * kChannelsPerFrame;
constexpr float kBufferTimeDurationSec = 0.01f;  // 10ms
constexpr size_t kBufferByteSize =
    kSampleRate * kBytesPerFrame * kBufferTimeDurationSec;
constexpr int kAudioSilenceThreshold = 0;

// Total delay: kBufferTimeDurationSec * kNumberBuffers
constexpr int kNumberBuffers = 2;

// A set to keep track of valid instances as we can't pass WeakPtr to the buffer
// callback.
class AudioCapturerInstanceSet {
 public:
  static base::Lock& GetLock();

  // Note: Add() and Remove() acquire a lock while Contains() doesn't.
  static void Add(AudioCapturerMac* instance);
  static void Remove(AudioCapturerMac* instance);
  static bool Contains(AudioCapturerMac* instance);

 private:
  friend class base::NoDestructor<AudioCapturerInstanceSet>;

  AudioCapturerInstanceSet();
  ~AudioCapturerInstanceSet();
  static AudioCapturerInstanceSet* Get();

  base::flat_set<AudioCapturerMac*> instance_set_;
  base::Lock lock_;
};

// static
base::Lock& AudioCapturerInstanceSet::GetLock() {
  return Get()->lock_;
}

// static
void AudioCapturerInstanceSet::Add(AudioCapturerMac* instance) {
  base::AutoLock guard(GetLock());
  Get()->instance_set_.insert(instance);
}

// static
void AudioCapturerInstanceSet::Remove(AudioCapturerMac* instance) {
  base::AutoLock guard(GetLock());
  Get()->instance_set_.erase(instance);
}

// static
bool AudioCapturerInstanceSet::Contains(AudioCapturerMac* instance) {
  return base::Contains(Get()->instance_set_, instance);
}

AudioCapturerInstanceSet::AudioCapturerInstanceSet() = default;

AudioCapturerInstanceSet::~AudioCapturerInstanceSet() = default;

// static
AudioCapturerInstanceSet* AudioCapturerInstanceSet::Get() {
  static base::NoDestructor<AudioCapturerInstanceSet> instance_set;
  return instance_set.get();
}

}  // namespace

// static
std::vector<AudioCapturerMac::AudioDeviceInfo>
AudioCapturerMac::GetAudioDevices() {
  AudioObjectPropertyAddress property_address;
  property_address.mScope = kAudioObjectPropertyScopeGlobal;
  property_address.mElement = kAudioObjectPropertyElementMain;

  UInt32 property_size;

  // Get all audio device IDs (which are UInt32).
  property_address.mSelector = kAudioHardwarePropertyDevices;
  OSStatus result = AudioObjectGetPropertyDataSize(
      kAudioObjectSystemObject, &property_address, 0, NULL, &property_size);
  if (result != noErr) {
    LOG(ERROR)
        << "AudioObjectGetPropertyDataSize(kAudioHardwarePropertyDevices) "
        << "failed. Error: " << result;
    return {};
  }

  UInt32 num_devices = property_size / sizeof(AudioDeviceID);
  auto device_ids = std::make_unique<AudioDeviceID[]>(num_devices);
  result =
      AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0,
                                 NULL, &property_size, device_ids.get());
  if (result != noErr) {
    LOG(ERROR) << "AudioObjectGetPropertyData(kAudioHardwarePropertyDevices) "
               << "failed. Error: " << result;
    return {};
  }

  std::vector<AudioDeviceInfo> audio_devices;

  for (UInt32 i = 0u; i < num_devices; i++) {
    AudioDeviceInfo audio_device;
    AudioDeviceID device_id = device_ids.get()[i];

    // Get the device name.
    property_address.mSelector = kAudioObjectPropertyName;
    base::apple::ScopedCFTypeRef<CFStringRef> device_name;
    property_size = sizeof(CFStringRef);
    result = AudioObjectGetPropertyData(device_id, &property_address, 0, NULL,
                                        &property_size,
                                        device_name.InitializeInto());
    if (result != noErr) {
      LOG(ERROR) << "AudioObjectGetPropertyData(" << device_id
                 << ", kAudioObjectPropertyName) "
                 << "failed. Error: " << result;
      continue;
    }
    audio_device.device_name = base::SysCFStringRefToUTF8(device_name.get());

    // Now find out its UID.
    property_address.mSelector = kAudioDevicePropertyDeviceUID;
    base::apple::ScopedCFTypeRef<CFStringRef> device_uid;
    property_size = sizeof(CFStringRef);
    result =
        AudioObjectGetPropertyData(device_id, &property_address, 0, NULL,
                                   &property_size, device_uid.InitializeInto());
    if (result != noErr) {
      LOG(ERROR) << "AudioObjectGetPropertyData(" << device_id
                 << ", kAudioDevicePropertyDeviceUID) "
                 << "failed. Error: " << result;
      continue;
    }
    audio_device.device_uid = base::SysCFStringRefToUTF8(device_uid.get());
    audio_devices.push_back(audio_device);
  }
  return audio_devices;
}

AudioCapturerMac::AudioCapturerMac(const std::string& audio_device_uid)
    : audio_device_uid_(audio_device_uid),
      silence_detector_(kAudioSilenceThreshold) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!audio_device_uid.empty());

  stream_description_.mSampleRate = kSampleRate;
  stream_description_.mFormatID = kAudioFormatLinearPCM;
  stream_description_.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  stream_description_.mBytesPerPacket = kBytesPerFrame;
  stream_description_.mFramesPerPacket = 1;
  stream_description_.mBytesPerFrame = kBytesPerFrame;
  stream_description_.mChannelsPerFrame = kChannelsPerFrame;
  stream_description_.mBitsPerChannel = 8 * kBytesPerChannel;
  stream_description_.mReserved = 0;

  AudioCapturerInstanceSet::Add(this);
}

AudioCapturerMac::~AudioCapturerMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AudioCapturerInstanceSet::Remove(this);

  DisposeInputQueue();
}

bool AudioCapturerMac::Start(const PacketCapturedCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_);
  DCHECK(callback);

  caller_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

  if (!StartInputQueue()) {
    return false;
  }

  callback_ = callback;
  return true;
}

// static
void AudioCapturerMac::HandleInputBufferOnAQThread(
    void* user_data,
    AudioQueueRef aq,
    AudioQueueBufferRef buffer,
    const AudioTimeStamp* start_time,
    UInt32 num_packets,
    const AudioStreamPacketDescription* packet_descs) {
  AudioCapturerMac* capturer = reinterpret_cast<AudioCapturerMac*>(user_data);

  {
    base::AutoLock guard(AudioCapturerInstanceSet::GetLock());
    if (!AudioCapturerInstanceSet::Contains(capturer)) {
      // The capturer has been destroyed.
      return;
    }
    capturer->caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioCapturerMac::HandleInputBuffer,
                       capturer->weak_factory_.GetWeakPtr(), aq, buffer));
  }
}

void AudioCapturerMac::HandleInputBuffer(AudioQueueRef aq,
                                         AudioQueueBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_started_) {
    LOG(WARNING) << "Playback has been stopped.";
    return;
  }

  DCHECK_EQ(input_queue_, aq);
  DCHECK(callback_);

  if (!silence_detector_.IsSilence(
          reinterpret_cast<const int16_t*>(buffer->mAudioData),
          buffer->mAudioDataByteSize / sizeof(int16_t) / kChannelsPerFrame)) {
    auto packet = std::make_unique<AudioPacket>();
    packet->add_data(buffer->mAudioData, buffer->mAudioDataByteSize);
    packet->set_encoding(AudioPacket::ENCODING_RAW);
    packet->set_sampling_rate(kSampleRate);
    packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
    packet->set_channels(AudioPacket::CHANNELS_STEREO);
    callback_.Run(std::move(packet));
  }

  // Recycle the buffer.
  // Only the first 2 params are needed for recording.
  OSStatus err = AudioQueueEnqueueBuffer(input_queue_, buffer, 0, NULL);
  HandleError(err, "AudioQueueEnqueueBuffer");
}

bool AudioCapturerMac::StartInputQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!input_queue_);
  DCHECK(!is_started_);

  if (mac::CanCaptureAudio()) {
    HOST_LOG << "Audio capture is allowed.";
  } else {
    HOST_LOG << "We have no audio capture permission. Requesting one...";
    mac::RequestAudioCapturePermission(base::BindOnce([](bool granted) {
      // We don't need to defer the AudioQueue setup process as the buffers will
      // start being filled up immediately after the user approves the request.
      if (granted) {
        HOST_LOG << "Audio capture permission granted.";
      } else {
        LOG(ERROR) << "Audio capture permission not granted.";
      }
    }));
  }

  // Setup input queue.
  // This runs on AudioQueue's internal thread. For some reason if we specify
  // inCallbackRunLoop to current thread, then the callback will never get
  // called.
  OSStatus err =
      AudioQueueNewInput(&stream_description_, &HandleInputBufferOnAQThread,
                         /* inUserData= */ this, /* inCallbackRunLoop= */ NULL,
                         kCFRunLoopCommonModes, 0, &input_queue_);

  if (HandleError(err, "AudioQueueNewInput")) {
    return false;
  }

  // Use the loopback device for input.
  HOST_LOG << "Using loopback device: " << audio_device_uid_;
  base::apple::ScopedCFTypeRef<CFStringRef> device_uid =
      base::SysUTF8ToCFStringRef(audio_device_uid_);
  CFStringRef unowned_device_uid = device_uid.get();
  err = AudioQueueSetProperty(input_queue_, kAudioQueueProperty_CurrentDevice,
                              &unowned_device_uid, sizeof(unowned_device_uid));
  if (HandleError(err,
                  "AudioQueueSetProperty(kAudioQueueProperty_CurrentDevice)")) {
    return false;
  }

  // Setup buffers.
  for (int i = 0; i < kNumberBuffers; i++) {
    // |buffer| will automatically be freed when |input_queue_| is released.
    AudioQueueBufferRef buffer;
    err = AudioQueueAllocateBuffer(input_queue_, kBufferByteSize, &buffer);
    if (HandleError(err, "AudioQueueAllocateBuffer")) {
      return false;
    }
    err = AudioQueueEnqueueBuffer(input_queue_, buffer, 0, NULL);
    if (HandleError(err, "AudioQueueEnqueueBuffer")) {
      return false;
    }
  }

  // Start input queue.
  err = AudioQueueStart(input_queue_, NULL);
  if (err == kAudioQueueErr_InvalidDevice) {
    LOG(ERROR) << "Loopback device " << audio_device_uid_
               << " could not be located";
    return false;
  }
  if (HandleError(err, "AudioQueueStart")) {
    return false;
  }
  is_started_ = true;

  silence_detector_.Reset(kSampleRate, kChannelsPerFrame);

  return true;
}

void AudioCapturerMac::DisposeInputQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!input_queue_) {
    return;
  }

  OSStatus err;

  if (is_started_) {
    err = AudioQueueStop(input_queue_, /* Immediate */ true);
    if (err != noErr) {
      LOG(DFATAL) << "Failed to call AudioQueueStop, error code: " << err;
    }
    is_started_ = false;
  }

  err = AudioQueueDispose(input_queue_, /* Immediate */ true);
  if (err != noErr) {
    LOG(DFATAL) << "Failed to call AudioQueueDispose, error code: " << err;
  }
  input_queue_ = nullptr;
}

bool AudioCapturerMac::HandleError(OSStatus err, const char* function_name) {
  if (err != noErr) {
    LOG(DFATAL) << "Failed to call " << function_name
                << ", error code: " << err;
    DisposeInputQueue();
    return true;
  }
  return false;
}

// AudioCapturer

bool AudioCapturer::IsSupported() {
  if (HostSettings::GetInstance()
          ->GetString(kMacAudioCaptureDeviceUid)
          .empty()) {
    HOST_LOG << kMacAudioCaptureDeviceUid << " is not set or not a string. "
             << "Audio capturer will be disabled.";
    return false;
  }
  HOST_LOG << kMacAudioCaptureDeviceUid
           << " is set. Audio capturer will be enabled.";
  return true;
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  std::string device_uid =
      HostSettings::GetInstance()->GetString(kMacAudioCaptureDeviceUid);
  if (device_uid.empty()) {
    // AudioCapturer::Create is still called even when IsSupported() returns
    // false.
    return nullptr;
  }
  return std::make_unique<AudioCapturerMac>(device_uid);
}

}  // namespace remoting
