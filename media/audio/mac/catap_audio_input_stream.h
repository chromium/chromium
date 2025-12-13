// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_CATAP_AUDIO_INPUT_STREAM_H_
#define MEDIA_AUDIO_MAC_CATAP_AUDIO_INPUT_STREAM_H_

#include <CoreAudio/CATapDescription.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/apple/glitch_helper.h"
#include "media/audio/audio_io.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"

@class NSError;

namespace media {

class CatapApi;
class PropertyListenerHelper;

// Captures audio loopback using the CoreAudio API for macOS 14.2
// and later. Used in a `CatapAudioInputStream` to provide the audio stream.
// The current implementation supports mono and stereo capture.
class MEDIA_EXPORT API_AVAILABLE(macos(14.2)) CatapAudioInputStreamSource {
 public:
  // Interface for listening to audio property changes. It's safe to call delete
  // on the `CatapAudioInputStreamSource` in the callbacks.
  class AudioPropertyChangeCallback {
   public:
    virtual void OnSampleRateChange() = 0;
    virtual void OnDefaultDeviceChange() = 0;
  };

  struct Config {
    Config(const AudioParameters& params,
           const std::string& device_id,
           bool force_mono_capture);

    int catap_channels;
    int output_channels;
    int sample_rate;
    int frames_per_buffer;
    bool capture_default_device;
    bool mute_local_device;
    bool exclude_chrome;
    std::optional<pid_t> capture_application_process_id;

    // Returns a human-readable string describing |*this|.  For debugging & test
    // output only.
    std::string AsHumanReadableString() const;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class OpenStatus {
    kOk = 0,
    kErrorDeviceAlreadyOpen = 1,
    kErrorCreatingProcessTap = 2,
    kErrorCreatingAggregateDevice = 3,
    kErrorCreatingIOProcID = 4,
    kErrorMissingAudioTapPermission = 5,
    kGetProcessAudioDeviceIdsReturnedEmpty = 6,
    kErrorConfiguringSampleRate = 7,
    kErrorConfiguringFramesPerBuffer = 8,
    kErrorCreatingTapDescription = 9,
    kGetDefaultDeviceUidEmpty = 10,
    kMaxValue = kGetDefaultDeviceUidEmpty
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CloseStatus {
    kOk = 0,
    kErrorDestroyingIOProcID = 1,
    kErrorDestroyingAggregateDevice = 2,
    kErrorDestroyingProcessTap = 3,
    kMaxValue = kErrorDestroyingProcessTap
  };

  // Only mono or stereo channels are supported for loopback device
  // compatibility.
  CatapAudioInputStreamSource(const raw_ptr<CatapApi> catap_api,
                              const Config& config,
                              const AudioManager::LogCallback log_callback,
                              const raw_ptr<AudioPropertyChangeCallback>
                                  audio_property_change_callback);

  CatapAudioInputStreamSource(const CatapAudioInputStreamSource&) = delete;
  CatapAudioInputStreamSource(CatapAudioInputStreamSource&&) = delete;
  CatapAudioInputStreamSource(const CatapAudioInputStreamSource&&) = delete;
  CatapAudioInputStreamSource& operator=(const CatapAudioInputStreamSource&) =
      delete;
  CatapAudioInputStreamSource& operator=(CatapAudioInputStreamSource&&) =
      delete;
  CatapAudioInputStreamSource& operator=(const CatapAudioInputStreamSource&&) =
      delete;

  ~CatapAudioInputStreamSource();

  // Implements the contract of AudioInputStream::Open() (including return
  // values). Prepares the `CatapAudioInputStreamSource` for recording and must
  // be called before Start().
  // `default_output_device_uid` is the unique ID (UID) for the default
  // output device. If the caller is unable to retrieve the UID, it can call
  // Open() with nullopt. In that case Open() will fail unless
  // the `CatapAudioInputStreamSource` captures from all output devices.
  AudioInputStream::OpenOutcome Open(
      std::optional<std::string> default_output_device_uid);

  // Starts the capture of system audio. The OnData callback is called when
  // audio data is available.
  void Start(AudioInputStream::AudioInputCallback* callback);

  // Stops the audio capture. When this returns, it's guaranteed that no more
  // calls to 'AudioInputCallback' will occur. The stream can be restarted
  // with a call to Start(). Stop() must be called before the destructor.
  void Stop();

  void OnCatapSample(const AudioBuffer* input_buffer,
                     const AudioTimeStamp* input_time);
  void OnError();

 private:
  void Close();

  // Returns all CoreAudio process audio device IDs that belong to the specified
  // process ID.
  NSArray<NSNumber*>* GetProcessAudioDeviceIds(pid_t chrome_process_id);

  // Configure the sample rate of the aggregate device according to `params_`.
  bool ConfigureSampleRateOfAggregateDevice();

  // Returns the sample rate of the aggregate device. Returns nullopt if the
  // sample rate could not be retrieved.
  std::optional<double> GetSampleRateOfAggregateDevice();

  // Configure the frames per buffer of the aggregate device according to
  // `params_`.
  bool ConfigureFramesPerBufferOfAggregateDevice();

  // Probe audio tap permission by getting and setting
  // AudioTapPropertyDescription. If either of these operations fail, this
  // function returns false which is an indication that we don't have system
  // audio capture permission.
  bool ProbeAudioTapPermissions();

  void ProcessPropertyChange(
      base::span<const AudioObjectPropertyAddress> property_addresses);

  // Send log messages to the stream creator.
  void SendLogMessage(const char* format, ...);

  // Called from the dtor and when the stream is reset.
  void ReportAndResetStats();

  // Interface used to access the CoreAudio framework.
  const raw_ptr<CatapApi> catap_api_;

  const Config config_;

  // The length of time covered by the audio data in a single audio buffer.
  const base::TimeDelta buffer_frames_duration_;

  // Used to detect and report glitches.
  GlitchHelper glitch_helper_;

  // Audio bus used to pass audio samples to sink_.
  const std::unique_ptr<AudioBus> audio_bus_;

  // Receives the processed audio data and errors. sink_ is set in the call to
  // Start() and must not be modified until Stop() is called where the audio
  // capture is stopped. While the capture is running, sink_ is accessed on a
  // thread that is associated with the capturer.
  raw_ptr<AudioInputStream::AudioInputCallback> sink_;

  // The next expected capture time is used as a fallback if the metadata in the
  // callback is missing a host time stamp. Only accessed from the capture
  // thread.
  std::optional<base::TimeTicks> next_expected_capture_time_;

  // Counter to track the number of callbacks with a missing host time stamp.
  // Incremented from the capture thread. Used to calculate statistics of
  // callbacks with missing host time when the capture has stopped.
  int callbacks_with_missing_host_time_ = 0;

  // Total number of callbacks, used to calculate the ratio of callbacks with
  // missing host time stamp. Incremented from the capture thread. Used to
  // calculate statistics of callbacks with missing host time when the capture
  // has stopped.
  int total_callbacks_ = 0;

  // True if we have received a callback with host time after there's been at
  // least one callback without host time. Changed from the capture thread while
  // the capture is running, and then accessed from the main sequence once the
  // capture has stopped.
  bool recovered_from_missing_host_time_ = false;

  // Total number of callbacks with a channel count mismatch. Incremented from
  // the capture thread. Used to report statistics of callbacks with channel
  // count mismatch when the capture has stopped.
  int total_callbacks_with_channel_count_mismatch_ = 0;

  // Total number of callbacks with a frames mismatch. Incremented from the
  // capture thread. Used to report statistics of callbacks with frames mismatch
  // when the capture has stopped.
  int total_callbacks_with_frames_mismatch_ = 0;

  // Callback to send log messages to the client.
  AudioManager::LogCallback log_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<PropertyListenerHelper> property_listener_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback called on audio device property changes.
  raw_ptr<AudioPropertyChangeCallback> audio_property_change_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  AudioObjectID aggregate_device_id_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kAudioObjectUnknown;
  AudioDeviceIOProcID tap_io_proc_id_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  AudioObjectID tap_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kAudioObjectUnknown;
  CATapDescription* __strong tap_description_
      GUARDED_BY_CONTEXT(sequence_checker_) = nil;
  bool is_device_open_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CatapAudioInputStreamSource> weak_ptr_factory_{this};
};

// `AudioInputStream` implementation that streams system output audio, that is
// captured by a `CatapAudioInputStreamSource`. This stream is responsible for
// providing a seamless stream to the listener, even if there are changes to the
// default output device, achieved by reinitializing the
// `CatapAudioInputStreamSource` as needed.
//
// Overview of operation:
// - An instance of `CatapAudioInputStream` is created by `AudioManagerMac`.
// - Open() is called. An `CatapAudioInputStreamSource` is created and opened,
//   creating the underlying audio tap and aggregate device.
// - Start(sink) is called, causing the stream to start delivering samples.
// - Audio samples are being received by
//   CatapAudioInputStreamSource::OnCatapSample() and forwarded to the
//   AudioInputCallback::OnData(). The audio tap is setup to capture audio from
//   the default output audio device unless the device ID is
//   kLoopbackAllDevicesId or the feature kMacCatapCaptureAllDevices is
//   enabled, in which case all output devices are captured.
// - Stop() is called, causing the stream to stop.
// - Close() is called, causing the stream output to be removed and the stream
//   to be destroyed.
class API_AVAILABLE(macos(14.2)) CatapAudioInputStream
    : public AgcAudioStream<AudioInputStream>,
      public CatapAudioInputStreamSource::AudioPropertyChangeCallback {
 public:
  struct MEDIA_EXPORT AudioDeviceIds {
    AudioDeviceIds();
    AudioDeviceIds(AudioDeviceID device_id, std::string uid);
    ~AudioDeviceIds();
    AudioDeviceIds(const AudioDeviceIds& other);
    std::optional<AudioDeviceID> id;
    std::optional<std::string> uid;
  };
  using NotifyOnCloseCallback = base::OnceCallback<void(AudioInputStream*)>;
  using GetDefaultDeviceIdsCallback = base::RepeatingCallback<AudioDeviceIds()>;

  CatapAudioInputStream(
      std::unique_ptr<CatapApi> catap_api,
      GetDefaultDeviceIdsCallback get_default_device_ids_callback,
      const AudioParameters& params,
      const std::string& device_id,
      AudioManager::LogCallback log_callback,
      const NotifyOnCloseCallback close_callback);

  // AudioInputStream:: implementation.
  AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // CatapAudioInputStreamSource::AudioPropertyChangeCallback implementation
  void OnSampleRateChange() override;
  void OnDefaultDeviceChange() override;

  ~CatapAudioInputStream() override;

 private:
  // Logs an error message and call OnError() on the `AudioInputCallback`.
  void OnError();

  // Restarts the `CatapAudioInputStreamSource`, and make sure the new Source
  // is put in the correct state.
  void RestartStream();

  int GetVirtualFormatChannels(AudioDeviceID device_id);

  // Send log messages to the client.
  void SendLogMessage(const char* format, ...);

  // Interface used to access the CoreAudio framework.
  const std::unique_ptr<CatapApi> catap_api_;

  // Audio parameters passed to the constructor. The parameters are only used
  // by `CatapAudioInputStreamSource`. `CatapAudioInputStream` keeps a copy to
  // be able to provide it to a new `CatapAudioInputStreamSource`, if `source_`
  // needs to be reinitialized.
  const AudioParameters params_;

  // One of AudioDeviceDescription::kLoopback*.
  const std::string device_id_;

  const bool restart_on_device_change_;

  // Called when the stream is closed and can be safely deleted.
  NotifyOnCloseCallback close_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback to send log messages to the client.
  AudioManager::LogCallback log_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Function that provide default output audio device IDs.
  GetDefaultDeviceIdsCallback get_default_device_ids_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Receives the processed audio data and errors. `audio_input_callback_` is
  // set in the call to Start() and is passed on to the
  // `CatapAudioInputSource`. `CatapAudioInputStream` keeps a copy to be able
  // to reinitialze `CatapAudioInputSource` if needed, and to be able to report
  // errors. Set to nullptr by Stop(). If `audio_input_callback_` is
  // non-nullptr, the stream is in Started state.
  raw_ptr<AudioInputStream::AudioInputCallback> audio_input_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The `CatapAudioInputStreamSource` that captures system audio that the
  // `CatapAudioInputStream` streams. `CatapAudioInputStream` may reinitialze
  // `source_` on default devices changes. If `source_` is non-nullptr, the
  // stream is in Open or Started state.
  std::unique_ptr<CatapAudioInputStreamSource> source_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

API_AVAILABLE(macos(14.2))
AudioInputStream* MEDIA_EXPORT CreateCatapAudioInputStreamForTesting(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    base::OnceCallback<void(AudioInputStream*)> close_callback,
    std::unique_ptr<CatapApi> catap_api,
    base::RepeatingCallback<CatapAudioInputStream::AudioDeviceIds()>
        get_default_device_ids_callback);

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_CATAP_AUDIO_INPUT_STREAM_H_
