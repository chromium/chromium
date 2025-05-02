// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>
#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "media/audio/apple/audio_auhal.h"
#include "media/audio/apple/audio_manager_apple.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/mac/audio_device_listener_mac.h"

namespace base {

namespace apple {
class ScopedObjCClassSwizzler;
}  // namespace apple
}  // namespace base

namespace media {

class AUAudioInputStream;
class AUHALStream;

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerMac : public AudioManagerApple {
 public:
  AudioManagerMac(std::unique_ptr<AudioThread> audio_thread,
                  AudioLogFactory* audio_log_factory);

  AudioManagerMac(const AudioManagerMac&) = delete;
  AudioManagerMac& operator=(const AudioManagerMac&) = delete;

  ~AudioManagerMac() override;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
  const char* GetName() override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;

  // Used to track destruction of input and output streams.
  void ReleaseOutputStream(AudioOutputStream* stream) override;
  void ReleaseInputStream(AudioInputStream* stream) override;

  // Changes the I/O buffer size for |device_id| if |desired_buffer_size| is
  // lower than the current device buffer size. The buffer size can also be
  // modified under other conditions. See comments in the corresponding cc-file
  // for more details.
  // Returns false if an error occurred.
  bool MaybeChangeBufferSize(AudioDeviceID device_id,
                             AudioUnit audio_unit,
                             AudioUnitElement element,
                             size_t desired_buffer_size) override;
  base::TimeDelta GetDeferStreamStartTimeout() const override;
  void StopAmplitudePeakTrace() override;

  // Implementation of AudioManagerApple

  // Returns the maximum microphone analog volume or 0.0 if device does not
  // have volume control.
  double GetMaxInputVolume(AudioDeviceID device_id) override;

  // Sets the microphone analog volume, with range [0.0, 1.0] inclusive.
  void SetInputVolume(AudioDeviceID device_id, double volume) override;

  // Returns the microphone analog volume, with range [0.0, 1.0] inclusive.
  double GetInputVolume(AudioDeviceID device_id) override;

  // Returns the current muting state for the microphone.
  bool IsInputMuted(AudioDeviceID device_id) override;

  // Retrieves the current hardware sample rate associated with a specified
  // device.
  int HardwareSampleRateForDevice(AudioDeviceID device_id) override;

  // If successful, this function returns no error and populates the out
  // parameter `input_format` with a valid ASBD. Otherwise, an error status code
  // will be returned.
  OSStatus GetInputDeviceStreamFormat(
      AudioUnit audio_unit,
      AudioStreamBasicDescription* input_format) override;

  static bool GetDefaultInputDevice(AudioDeviceID* input_device);
  static bool GetDefaultOutputDevice(AudioDeviceID* output_device);
  static AudioDeviceID GetAudioDeviceIdByUId(bool is_input,
                                             const std::string& device_id);

  // Finds the first subdevice, in an aggregate device, with output streams.
  static AudioDeviceID FindFirstOutputSubdevice(
      AudioDeviceID aggregate_device_id);

  // Returns a vector with the IDs of all devices related to the given
  // |device_id|. The vector is empty if there are no related devices or
  // if there is an error.
  std::vector<AudioObjectID> GetRelatedDeviceIDs(AudioObjectID device_id);

  // OSX has issues with starting streams as the system goes into suspend and
  // immediately after it wakes up from resume.  See http://crbug.com/160920.
  // As a workaround we delay Start() when it occurs after suspend and for a
  // small amount of time after resume.
  //
  // Streams should consult ShouldDeferStreamStart() and if true check the value
  // again after |kStartDelayInSecsForPowerEvents| has elapsed. If false, the
  // stream may be started immediately.
  // TODO(henrika): track UMA statistics related to defer start to come up with
  // a suitable delay value.
  enum { kStartDelayInSecsForPowerEvents = 5 };
  bool ShouldDeferStreamStart() const override;

  // True if the device is on battery power.
  bool IsOnBatteryPower() const;

  // Number of times the device has resumed from power suspension.
  size_t GetNumberOfResumeNotifications() const;

  // True if the device is suspending.
  bool IsSuspending() const;

  // Number of constructed output and input streams.
  size_t output_streams() const { return output_streams_.size(); }
  size_t low_latency_input_streams() const {
    return low_latency_input_streams_.size();
  }
  size_t basic_input_streams() const { return basic_input_streams_.size(); }

  // Manage device capabilities for ambient noise reduction. These functionality
  // currently implemented on the Mac platform.
  bool DeviceSupportsAmbientNoiseReduction(AudioDeviceID device_id) override;
  bool SuppressNoiseReduction(AudioDeviceID device_id) override;
  void UnsuppressNoiseReduction(AudioDeviceID device_id) override;

  // The state of a single device for which we've tried to disable Ambient Noise
  // Reduction. If the device initially has ANR enabled, it will be turned off
  // as the suppression count goes from 0 to 1 and turned on again as the count
  // returns to 0.
  struct NoiseReductionState {
    enum State { DISABLED, ENABLED };
    State initial_state = DISABLED;
    int suppression_count = 0;
  };

  // Keep track of the devices that we've changed the Ambient Noise Reduction
  // setting on.
  std::map<AudioDeviceID, NoiseReductionState> device_noise_reduction_states_;

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

  // Returns a vector with the IDs of all bluetooth devices related to the given
  // |device_id|, which is also a bluetooth device. The vector is empty if there
  // are no related devices or if there is an error.
  std::vector<AudioObjectID> GetRelatedBluetoothDeviceIDs(
      AudioObjectID device_id);

  // Virtual for testing.

  // Returns a vector with the IDs of all audio devices in the system.
  // The vector is empty if there are no devices or if there is an error.
  virtual std::vector<AudioObjectID> GetAllAudioDeviceIDs();

  // Returns a vector with the IDs of all non-bluetooth devices related to the
  // given |device_id|, which is also a non-bluetooth device. The vector is
  // empty if there are no related devices or if there is an error.
  virtual std::vector<AudioObjectID> GetRelatedNonBluetoothDeviceIDs(
      AudioObjectID device_id);

  // Returns a string with a unique device ID for the given |device_id|, or no
  // value if there is an error.
  virtual std::optional<std::string> GetDeviceUniqueID(AudioObjectID device_id);

  // Returns the transport type of the given |device_id|, or no value if
  // |device_id| has no source or if there is an error.
  virtual std::optional<uint32_t> GetDeviceTransportType(
      AudioObjectID device_id);
  void ShutdownOnAudioThread() override;

 private:
  void InitializeOnAudioThread();

  int ChooseBufferSize(bool is_input, int sample_rate);

  // Notify streams of a device change if the default output device or its
  // sample rate has changed, otherwise does nothing.
  void HandleDeviceChanges();

  // Helper function to check if the volume control is available on specific
  // channel of a device.
  static bool IsVolumeSettableOnChannel(AudioDeviceID device_id, int channel);

  // Return the number of channels in each frame of audio data, which is used
  // when querying the volume of each channel.
  static int GetNumberOfChannelsForDevice(AudioDeviceID device_id);

  std::string GetDefaultDeviceID(bool is_input);

  std::unique_ptr<AudioDeviceListenerMac> output_device_listener_;

  // Track the output sample-rate and the default output device
  // so we can intelligently handle device notifications only when necessary.
  int current_sample_rate_;
  AudioDeviceID current_output_device_;

  // Helper class which monitors power events to determine if output streams
  // should defer Start() calls.  Required to workaround an OSX bug.  See
  // http://crbug.com/160920 for more details.
  class AudioPowerObserver;
  std::unique_ptr<AudioPowerObserver> power_observer_;

  // Tracks all constructed input and output streams.
  // TODO(alokp): We used to track these streams to close before destruction.
  // We no longer close the streams, so we may be able to get rid of these
  // member variables. They are currently used by MaybeChangeBufferSize().
  // Investigate if we can remove these.
  std::unordered_set<AudioInputStream*> basic_input_streams_;
  std::unordered_set<AUAudioInputStream*> low_latency_input_streams_;
  std::unordered_set<AUHALStream*> output_streams_;

  // Used to swizzle SCStreamManager when performing loopback capture.
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      screen_capture_kit_swizzler_;

  // Set to true in the destructor. Ensures that methods that touches native
  // Core Audio APIs are not executed during shutdown.
  bool in_shutdown_;

  base::WeakPtrFactory<AudioManagerMac> weak_ptr_factory_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
