// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/requires_api.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/android/aaudio_bluetooth_output.h"
#include "media/audio/android/aaudio_input.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/android/audio_device_id.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_parameters.h"

namespace media {

class MuteableAudioOutputStream;

// Android implementation of AudioManager.
class MEDIA_EXPORT AudioManagerAndroid : public AudioManagerBase {
 public:
  struct JniAudioDevice {
   public:
    JniAudioDevice(int id, std::optional<std::string> name, int type);

    JniAudioDevice(const JniAudioDevice&);
    JniAudioDevice& operator=(const JniAudioDevice&);
    JniAudioDevice(JniAudioDevice&&);
    JniAudioDevice& operator=(JniAudioDevice&&);

    ~JniAudioDevice();

    int id;
    std::optional<std::string> name;
    int type;
  };

  class JniDelegate {
   public:
    virtual ~JniDelegate() = default;

    // Returns metadata about the available audio devices as reported by the
    // Android framework, filtered to input devices if `inputs` is true, and to
    // output devices otherwise.
    virtual std::vector<JniAudioDevice> GetDevices(bool inputs) = 0;

    // Returns metadata about the available "synthetic" communication devices,
    // which abstractly represent an input/output audio device pair. If the
    // process lacks `MODIFY_AUDIO_SETTINGS` or `RECORD_AUDIO` permissions,
    // returns `std::nullopt` instead.
    virtual std::optional<std::vector<JniAudioDevice>>
    GetCommunicationDevices() = 0;

    virtual int GetMinInputFrameSize(int sample_rate, int channels) = 0;

    virtual bool AcousticEchoCancelerIsAvailable() = 0;

    virtual base::TimeDelta GetOutputLatency() = 0;

    virtual void SetCommunicationAudioModeOn(bool on) = 0;

    virtual bool SetCommunicationDevice(std::string_view device_id) = 0;

    // Gets whether Bluetooth SCO is currently enabled.
    virtual bool IsBluetoothScoOn() = 0;

    // Requests for Bluetooth SCO to be enabled or disabled. This request may
    // fail.
    virtual void MaybeSetBluetoothScoState(bool state) = 0;

    virtual int GetNativeOutputSampleRate() = 0;

    virtual bool IsAudioLowLatencySupported() = 0;

    virtual int GetAudioLowLatencyOutputFrameSize() = 0;

    virtual int GetMinOutputFrameSize(int sample_rate, int channels) = 0;

    // Returns a bitmask of audio encoding formats supported by all connected
    // HDMI output devices.
    virtual AudioParameters::Format GetHdmiOutputEncodingFormats() = 0;

    virtual int GetLayoutWithMaxChannels() = 0;
  };

  AudioManagerAndroid(std::unique_ptr<AudioThread> audio_thread,
                      AudioLogFactory* audio_log_factory);

  AudioManagerAndroid(const AudioManagerAndroid&) = delete;
  AudioManagerAndroid& operator=(const AudioManagerAndroid&) = delete;

  ~AudioManagerAndroid() override;

  void InitializeIfNeeded();

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;

  AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  void ReleaseOutputStream(AudioOutputStream* stream) override;
  void ReleaseInputStream(AudioInputStream* stream) override;
  const std::string_view GetName() override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeBitstreamOutputStream(
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

  void SetMute(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jboolean muted);

  // Sets a volume that applies to all this manager's output audio streams.
  // This overrides other SetVolume calls (e.g. through AudioHostMsg_SetVolume).
  // TODO(https://crbug.com/422733084): this functionality is likely unused.
  void SetOutputVolumeOverride(double volume);
  bool HasOutputVolumeOverride(double* out_volume) const;

  // Get the latency introduced by the hardware.  It relies on
  // AudioManager.getOutputLatency, which is both (a) hidden and (b) not
  // guaranteed to be meaningful.  Do not use this, except in the context of
  // b/80326798 to adjust (hackily) for hardware latency that OpenSLES isn't
  // otherwise accounting for.
  base::TimeDelta GetOutputLatency();

  // Returns a bitmask of audio encoding formats supported by all connected HDMI
  // output devices.
  static AudioParameters::Format GetHdmiOutputEncodingFormats();

  // Called by an `AAudioInputStream` when it is started, i.e. it begins
  // providing audio data.
  void REQUIRES_ANDROID_API(AAUDIO_MIN_API)
      OnStartAAudioInputStream(AAudioInputStream* stream);

  // Called by an `AAudioInputStream` when it is stopped, i.e. it stops
  // providing audio data.
  void REQUIRES_ANDROID_API(AAUDIO_MIN_API)
      OnStopAAudioInputStream(AAudioInputStream* stream);

  void SetJniDelegateForTesting(std::unique_ptr<JniDelegate> jni_delegate);

 protected:
  void ShutdownOnAudioThread() override;
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  using DeviceCache =
      base::flat_map<android::AudioDeviceId, android::AudioDevice>;
  using OutputStreams =
      base::flat_set<raw_ptr<MuteableAudioOutputStream, CtnExperimental>>;
  REQUIRES_ANDROID_API(AAUDIO_MIN_API)
  typedef base::flat_set<raw_ptr<AAudioBluetoothOutputStream, CtnExperimental>>
      BluetoothOutputStreams;  // `REQUIRES_ANDROID_API` appears to be
                               // incompatible with using-declarations.
  using InputStreams =
      base::flat_set<raw_ptr<AudioInputStream, CtnExperimental>>;

  enum class AudioDeviceDirection {
    kInput,   // Audio source
    kOutput,  // Audio sink
  };

  JniDelegate& GetJniDelegate();

  bool HasNoAudioInputStreams();
  void GetDeviceNames(AudioDeviceNames* device_names,
                      AudioDeviceDirection direction);
  void GetCommunicationDeviceNames(AudioDeviceNames* device_names);

  // Retrieve a mapping from device IDs to devices for the specified `direction`
  // which exclusively contains information about devices present during the
  // most recent call to `GetDeviceNames()` for the respective direction.
  const DeviceCache& GetDeviceCache(AudioDeviceDirection direction) const;

  // Utility for `Make(...)Stream` methods which retrieves an appropriate
  // `android::AudioDevice` based on the provided device ID string. Returns
  // `std::nullopt` if the device ID is valid but its corresponding device is
  // not available, which usually indicates that the device was disconnected.
  std::optional<android::AudioDevice> GetDeviceForAAudioStream(
      std::string_view id_string,
      AudioDeviceDirection direction);

  int GetOptimalOutputFrameSize(int sample_rate, int channels);
  ChannelLayoutConfig GetLayoutWithMaxChannels();

  void DoSetMuteOnAudioThread(bool muted);
  void DoSetVolumeOnAudioThread(double volume);

  std::unique_ptr<JniDelegate> jni_delegate_;

  // Most recently fetched device data. See `GetDeviceCache` for more details.
  DeviceCache input_device_cache_;
  DeviceCache output_device_cache_;

  OutputStreams output_streams_;
  REQUIRES_ANDROID_API(AAUDIO_MIN_API)
  BluetoothOutputStreams bluetooth_output_streams_;

  InputStreams input_streams_requiring_sco_;

  // Enabled when first input stream is created and set to false when last
  // input stream is destroyed. Also affects the stream type of output streams.
  bool communication_mode_is_on_;

  // If set, overrides volume level on output streams
  bool output_volume_override_set_;
  double output_volume_override_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
