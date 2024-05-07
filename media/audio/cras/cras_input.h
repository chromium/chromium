// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_CRAS_INPUT_H_
#define MEDIA_AUDIO_CRAS_CRAS_INPUT_H_

#include <cras_client.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "media/audio/aecdump_recording_manager.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_debug_recording_helper.h"
#include "media/audio/audio_io.h"
#include "media/audio/cras/audio_manager_cras_base.h"
#include "media/audio/system_glitch_reporter.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioManagerCrasBase;

// Provides an input stream for audio capture based on CRAS, the ChromeOS Audio
// Server.  This object is not thread safe and all methods should be invoked in
// the thread that created the object.
class MEDIA_EXPORT CrasInputStream : public AgcAudioStream<AudioInputStream>,
                                     public AecdumpRecordingSource {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // audio manager who is creating this object.
  CrasInputStream(const AudioParameters& params,
                  AudioManagerCrasBase* manager,
                  const std::string& device_id,
                  const AudioManager::LogCallback& log_callback);

  CrasInputStream(const CrasInputStream&) = delete;
  CrasInputStream& operator=(const CrasInputStream&) = delete;

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  ~CrasInputStream() override;

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

  // Implementation of AecdumpRecordingSource
  void StartAecdump(base::File aecdump_file) override;
  void StopAecdump() override;

 private:
  // Handles requests to get samples from the provided buffer.  This will be
  // called by the audio server when it has samples ready.
  static int SamplesReady(struct libcras_stream_cb_data* data);

  // Handles notification that there was an error with the playback stream.
  static int StreamError(cras_client* client,
                         cras_stream_id_t stream_id,
                         int err,
                         void* arg);

  // Reads one or more buffers of audio from the device, passes on to the
  // registered callback. Called from SamplesReady().
  void ReadAudio(size_t frames, uint8_t* buffer, const timespec* latency_ts);

  // Deals with an error that occurred in the stream.  Called from
  // StreamError().
  void NotifyStreamError(int err);

  // Convert from dB * 100 to a volume ratio.
  double GetVolumeRatioFromDecibels(double dB) const;

  // Convert from a volume ratio to dB.
  double GetDecibelsFromVolumeRatio(double volume_ratio) const;

  // Return true to use AEC in CRAS for this input stream.
  inline bool UseCrasAec() const;

  // Return true to use NS in CRAS for this input stream.
  inline bool UseCrasNs() const;

  // Return true to use AGC in CRAS for this input stream.
  inline bool UseCrasAgc() const;

  // Return true to use client controlled voice isolation in CRAS for this
  // input stream.
  inline bool UseClientControlledVoiceIsolation() const;

  // Return true to use voice isolation in CRAS for this input stream.
  inline bool UseCrasVoiceIsolation() const;

  // Return true to allow AEC on DSP for this input stream.
  inline bool DspBasedAecIsAllowed() const;

  // Return true to allow NS on DSP for this input stream.
  inline bool DspBasedNsIsAllowed() const;

  // Return true to allow AGC on DSP for this input stream.
  inline bool DspBasedAgcIsAllowed() const;

  // Return true if UI Gains should be ignored for this input stream.
  inline bool IgnoreUiGains() const;

  // Called from the dtor and when the stream is reset.
  void ReportAndResetStats();

  // There are 2 main reasons for input audio glitches.
  // 1. Audio frames are overwritten in the shared memory due to client being
  //    too slow at taking them out, and shared memory overrun occurs.
  //    The overwritten frames are calculated using |overrun_frames|.
  // 2. Audio samples are dropped from the input device hardware buffer due to
  //    there being too many unhandled samples. The duration of the dropped
  //    audio samples calculated using |dropped_samples_duration|.
  // Check if the input audio glitches of these 2 types happen this callback.
  // The total duration of the audio glitch for this callback is the
  // combination of the glitch duration from both types of input audio
  // glitches.
  void CalculateAudioGlitches(uint32_t overrun_frames,
                              base::TimeDelta dropped_samples_duration);

  // Non-refcounted pointer back to the audio manager.
  // The AudioManager indirectly holds on to stream objects, so we don't
  // want circular references.  Additionally, stream objects live on the
  // audio thread, which is owned by the audio manager and we don't want to
  // addref the manager from that thread.
  AudioManagerCrasBase* const audio_manager_;

  // Callback to pass audio samples too, valid while recording.
  AudioInputCallback* callback_ = NULL;

  // The client used to communicate with the audio server.
  struct libcras_client* client_ = NULL;

  // PCM parameters for the stream.
  const AudioParameters params_;

  // True if the stream has been started.
  bool started_ = false;

  // ID of the playing stream.
  cras_stream_id_t stream_id_ = 0;

  // Direction of the stream.
  const CRAS_STREAM_DIRECTION stream_direction_ = CRAS_STREAM_INPUT;

  // Index of the CRAS device to stream input from.
  int pin_device_ = NO_DEVICE;

  // True if the stream is a system-wide loopback stream.
  const bool is_loopback_;
  // True if the loopback stream does not contain chrome audio.
  const bool is_loopback_without_chrome_;

  // True if we want to mute system audio during capturing.
  const bool mute_system_audio_;
  bool mute_done_ = false;

#if DCHECK_IS_ON()
  // Flag to indicate if recording has been enabled or not.
  bool recording_enabled_;
#endif

  // Value of input stream volume, between 0.0 - 1.0.
  double input_volume_ = 1.0f;

  std::unique_ptr<AudioBus> audio_bus_;

  // Used to aggregate and report glitch metrics to UMA (periodically) and to
  // text logs (when a stream ends).
  SystemGlitchReporter glitch_reporter_;

  AudioGlitchInfo::Accumulator glitch_info_accumulator_;

  // Callback to send statistics info.
  const AudioManager::LogCallback log_callback_;

  // Contains the amount of overrun frames passed in from the previous callback.
  // Overrun frames are audio frames that are overwrittten in the shared memory
  // due to client delay.
  uint32_t last_overrun_frames_ = 0;

  // Contains the duration of dropped samples passed in from the previous
  // callback.
  // Dropped data are samples dropped from the input device's hardware buffer
  // due to too many samples.
  base::TimeDelta last_dropped_samples_duration_;

  AmplitudePeakDetector peak_detector_;

  base::WeakPtrFactory<CrasInputStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_INPUT_H_
