// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of AudioInputStream for Mac OS X using the special AUHAL
// input Audio Unit present in OS 10.4 and later.
// The AUHAL input Audio Unit is for low-latency audio I/O.
//
// Overview of operation:
//
// - An object of AUAudioInputStream is created by the AudioManager
//   factory: audio_man->MakeAudioInputStream().
// - Next some thread will call Open(), at that point the underlying
//   AUHAL output Audio Unit is created and configured.
// - Then some thread will call Start(sink).
//   Then the Audio Unit is started which creates its own thread which
//   periodically will provide the sink with more data as buffers are being
//   produced/recorded.
// - At some point some thread will call Stop(), which we handle by directly
//   stopping the AUHAL output Audio Unit.
// - The same thread that called stop will call Close() where we cleanup
//   and notify the audio manager, which likely will destroy this object.
//
// Implementation notes:
//
// - It is recommended to first acquire the native sample rate of the default
//   input device and then use the same rate when creating this object.
// - Calling Close() also leads to self destruction.
// - The latency consists of two parts:
//   1) Hardware latency, which includes Audio Unit latency, audio device
//      latency;
//   2) The delay between the actual recording instant and the time when the
//      data packet is provided as a callback.
//
#ifndef MEDIA_AUDIO_APPLE_AUDIO_LOW_LATENCY_INPUT_H_
#define MEDIA_AUDIO_APPLE_AUDIO_LOW_LATENCY_INPUT_H_

#include <AudioUnit/AudioUnit.h>

#include <memory>
#include <vector>

#include "base/atomicops.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/system_glitch_reporter.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"

#if BUILDFLAG(IS_MAC)
#include "media/audio/mac/audio_manager_mac.h"
#else
#include "media/audio/ios/audio_manager_ios.h"
#endif

namespace media {
class AudioManagerApple;

class MEDIA_EXPORT AUAudioInputStream
    : public AgcAudioStream<AudioInputStream> {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  AUAudioInputStream(
      AudioManagerApple* manager,
      const AudioParameters& input_params,
      AudioDeviceID audio_device_id,
      const AudioManager::LogCallback& log_callback,
      AudioManagerBase::VoiceProcessingMode voice_processing_mode);

  AUAudioInputStream(const AUAudioInputStream&) = delete;
  AUAudioInputStream& operator=(const AUAudioInputStream&) = delete;

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioInputStream::Close().
  ~AUAudioInputStream() override;

  // Implementation of AudioInputStream.
  AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // Returns true if the audio unit is active/running.
  // The result is based on the kAudioOutputUnitProperty_IsRunning property
  // which exists for output units.
  bool IsRunning();

  AudioDeviceID device_id() const { return input_device_id_; }
  size_t requested_buffer_size() const {
    return input_params_.frames_per_buffer();
  }
  AudioUnit audio_unit() const { return audio_unit_; }

  // Fan out the data from the first half of audio_buffer into interleaved
  // stereo across the whole of audio_buffer. Public for testing only.
  static void UpmixMonoToStereoInPlace(AudioBuffer* audio_buffer,
                                       int bytes_per_sample);

 private:
  bool OpenAUHAL();
  bool OpenVoiceProcessingAU();

  // Callback functions called on a real-time priority I/O thread from the audio
  // unit. These methods are called when recorded audio is available.
  static OSStatus DataIsAvailable(void* context,
                                  AudioUnitRenderActionFlags* flags,
                                  const AudioTimeStamp* time_stamp,
                                  UInt32 bus_number,
                                  UInt32 number_of_frames,
                                  AudioBufferList* io_data);
  OSStatus OnDataIsAvailable(AudioUnitRenderActionFlags* flags,
                             const AudioTimeStamp* time_stamp,
                             UInt32 bus_number,
                             UInt32 number_of_frames);

  // Pushes recorded data to consumer of the input audio stream.
  OSStatus Provide(UInt32 number_of_frames,
                   AudioBufferList* io_data,
                   const AudioTimeStamp* time_stamp);

  // Gets the current capture time.
  base::TimeTicks GetCaptureTime(const AudioTimeStamp* input_time_stamp);

  // Issues the OnError() callback to the |sink_|.
  void HandleError(OSStatus err, const base::Location& location = FROM_HERE);

  // Helper methods to set and get atomic |input_callback_is_active_|.
  void SetInputCallbackIsActive(bool active);
  bool GetInputCallbackIsActive();

  // Checks if a stream was started successfully and the audio unit also starts
  // to call InputProc() as it should. This method is called once when a timer
  // expires some time after calling Start().
  void CheckInputStartupSuccess();

  // Uninitializes the audio unit if needed.
  void CloseAudioUnit();

  // Reinitializes the AudioUnit to use a new output device.
  void ReinitializeVoiceProcessingAudioUnit();

  // Adds extra UMA stats when it has been detected that startup failed.
  void AddHistogramsForFailedStartup();

  // Updates capture timestamp, current lost frames, and total lost frames and
  // glitches.
  void UpdateCaptureTimestamp(const AudioTimeStamp* timestamp);

  // Called from the dtor and when the stream is reset.
  void ReportAndResetStats();

  // Verifies that Open(), Start(), Stop() and Close() are all called on the
  // creating thread which is the main browser thread (CrBrowserMain) on Mac.
  THREAD_CHECKER(thread_checker_);

  // Our creator, the audio manager needs to be notified when we close.
  const raw_ptr<AudioManagerApple> manager_;

  // The audio parameters requested when creating the stream.
  const AudioParameters input_params_;

  // Stores the number of frames that we actually get callbacks for.
  // This may be different from what we ask for, so we use this for stats in
  // order to understand how often this happens and what are the typical values.
  size_t number_of_frames_provided_;

  // Pointer to the object that will receive the recorded audio samples.
  raw_ptr<AudioInputCallback> sink_;

  // Structure that holds the desired output format of the stream.
  // Note that, this format can differ from the device(=input) format.
  AudioStreamBasicDescription format_;

  // The special Audio Unit called AUHAL, which allows us to pass audio data
  // directly from a microphone, through the HAL, and to our application.
  // The AUHAL also enables selection of non default devices.
  AudioUnit audio_unit_;

  // The UID refers to the current input audio device.
  const AudioDeviceID input_device_id_;

  // Provides a mechanism for encapsulating one or more buffers of audio data.
  AudioBufferList audio_buffer_list_;

  // Temporary storage for recorded data. The InputProc() renders into this
  // array as soon as a frame of the desired buffer size has been recorded.
  std::unique_ptr<uint8_t[]> audio_data_buffer_;

  // Fixed capture hardware latency.
  base::TimeDelta hardware_latency_;

  // FIFO used to accumulates recorded data.
  media::AudioBlockFifo fifo_;

  // Used to defer Start() to workaround http://crbug.com/160920.
  base::CancelableOnceClosure deferred_start_cb_;

  // Contains time of last successful call to AudioUnitRender().
  // Initialized first time in Start() and then updated for each valid
  // audio buffer. Used to detect long error sequences and to take actions
  // if length of error sequence is above a certain limit.
  base::TimeTicks last_success_time_;

  // Flags to indicate if we have gotten an input callback.
  // |got_input_callback_| is only accessed on the OS audio thread in
  // OnDataIsAvailable() and is set to true when the first callback comes. It
  // acts as a gate to only set |input_callback_is_active_| atomically once.
  // |got_input_callback_| is reset to false in Stop() on the main thread. This
  // is safe since after stopping the audio unit there is no current callback
  // ongoing and no further callbacks coming.
  bool got_input_callback_;
  base::subtle::Atomic32 input_callback_is_active_;

  // Timer which triggers CheckInputStartupSuccess() to verify that input
  // callbacks have started as intended after a successful call to Start().
  // This timer lives on the main browser thread.
  std::unique_ptr<base::OneShotTimer> input_callback_timer_;

  // Set to true when we've successfully called SuppressNoiseReduction to
  // disable ambient noise reduction.
  bool noise_reduction_suppressed_;

  // Controls whether or not we use the kAudioUnitSubType_VoiceProcessingIO
  // voice processing component that provides echo cancellation, ducking
  // and gain control on Sierra and later.
  const bool use_voice_processing_;

  // The of the output device to cancel echo from.
  AudioDeviceID output_device_id_for_aec_;

  // Stores the timestamp of the previous audio buffer provided by the OS.
  // We use this in combination with |last_number_of_frames_| to detect when
  // the OS has decided to skip providing frames (i.e. a glitch).
  // This can happen in case of high CPU load or excessive blocking on the
  // callback audio thread.
  // These variables are only touched on the callback thread and then read
  // in the dtor (when no longer receiving callbacks).
  // NOTE: Float64 and UInt32 types are used for native API compatibility.
  Float64 last_sample_time_;
  UInt32 last_number_of_frames_;

  // Used to aggregate and report glitch metrics to UMA (periodically) and to
  // text logs (when a stream ends).
  SystemGlitchReporter glitch_reporter_;

  // Used to accumulate glitches to be passed to the AudioInputCallback.
  AudioGlitchInfo::Accumulator glitch_accumulator_;

  AmplitudePeakDetector peak_detector_;

  // Callback to send statistics info.
  AudioManager::LogCallback log_callback_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_APPLE_AUDIO_LOW_LATENCY_INPUT_H_
