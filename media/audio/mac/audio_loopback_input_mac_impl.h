// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_LOOPBACK_INPUT_MAC_IMPL_H_
#define MEDIA_AUDIO_MAC_AUDIO_LOOPBACK_INPUT_MAC_IMPL_H_

#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/audio_io.h"
#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_parameters.h"

@class NSError;
@class SCContentFilter;
@class ScreenCaptureKitAudioHelper;
@class SCShareableContent;
@class SCStream;
@class SCStreamConfiguration;
@protocol SCStreamDelegate;
using CMSampleBufferRef = struct opaqueCMSampleBuffer*;

namespace media {

class SharedHelper;

// Implementation of AudioInputStream using the ScreenCaptureKit (SCK) API for
// macOS 13.0 and later, intended solely for system audio loopback capture.
//
// Overview of operation:
// - An instance of SCKAudioInputStream is created by AudioManagerMac.
// - Open() is called, prompting enumeration of available shareable content. The
//   function synchronously waits for the content to be enumerated and sets up
//   the stream with the created filter.
// - Start(sink) is called, causing the stream to start delivering samples.
// - Audio samples are being received by OnStreamSample() and forwarded to the
//   sink.
// - Stop() is called, causing the stream to stop.
// - Close() is called, causing the stream output to be removed and the stream
//   to be destroyed.
//
// API notes:
// - ScreenCaptureKit requires TCC screen capture permissions, which are granted
//   to the browser process and inherited by the audio service. For the
//   inheritance to work correctly, Chromium must be code signed.
// - The audio service sandbox requires +[SCStreamManager
//   requestUserPermissionForScreenCapture] to be swizzled so that it reports
//   that permissions have been granted. This is currently done in
//   AudioManagerMac.
class MEDIA_EXPORT API_AVAILABLE(macos(13.0)) SCKAudioInputStream
    : public AgcAudioStream<AudioInputStream> {
  using NotifyOnCloseCallback =
      base::RepeatingCallback<void(AudioInputStream*)>;
  using StartSCStreamMockingCallback =
      base::RepeatingCallback<void(SCStream*,
                                   SCContentFilter*,
                                   SCStreamConfiguration*,
                                   id<SCStreamDelegate>)>;

 public:
  SCKAudioInputStream(const AudioParameters& params,
                      const std::string& device_id,
                      const AudioManager::LogCallback log_callback,
                      const NotifyOnCloseCallback close_callback);
  SCKAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const AudioManager::LogCallback log_callback,
      const NotifyOnCloseCallback close_callback,
      const StartSCStreamMockingCallback start_scstream_mocking_callback,
      const base::TimeDelta shareable_content_timeout);

  SCKAudioInputStream(const SCKAudioInputStream&) = delete;
  SCKAudioInputStream(SCKAudioInputStream&&) = delete;
  SCKAudioInputStream(const SCKAudioInputStream&&) = delete;
  SCKAudioInputStream& operator=(const SCKAudioInputStream&) = delete;
  SCKAudioInputStream& operator=(SCKAudioInputStream&&) = delete;
  SCKAudioInputStream& operator=(const SCKAudioInputStream&&) = delete;

  ~SCKAudioInputStream() override;

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

 private:
  // Processes the audio data received from the system. Runs on a SCK thread.
  void OnStreamSample(
      base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer,
      const double volume);

  // Invoked when an error occurs while starting or running the stream. Runs on
  // a SCK thread.
  void OnStreamError();

  // Send log messages to the stream creator.
  void SendLogMessage(const char* format, ...);

  // Audio parameters passed to the constructor.
  const AudioParameters params_;

  // One of AudioDeviceDescription::kLoopback*.
  const std::string device_id_;

  // Wraps the non-interleaved audio buffer received from the system.
  const std::unique_ptr<AudioBus> audio_bus_;

  // Receives the processed audio data and errors. Must not be modified while
  // |shared_helper_| has callbacks set.
  raw_ptr<AudioInputCallback> sink_;

  // Callback to send log messages to the client.
  AudioManager::LogCallback log_callback_;

  // Called when the stream is closed and can be safely deleted.
  const NotifyOnCloseCallback close_callback_;

  // Used by tests to get notified of a new SCStream creation.
  StartSCStreamMockingCallback start_scstream_mocking_callback_;

  // Refcounted helper which helps us sync operation between browser and SCK
  // threads.
  scoped_refptr<SharedHelper> shared_helper_;

  // Stream output and delegate registered with the API to receive and handle
  // samples and errors.
  ScreenCaptureKitAudioHelper* __strong sck_helper_;

  // The stream object created by the API.
  SCStream* __strong stream_;

  // Serial queue used by the API for new samples.
  dispatch_queue_t __strong queue_;

  // The length of time covered by the audio data in a single audio buffer.
  const base::TimeDelta buffer_frames_duration_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_LOOPBACK_INPUT_MAC_IMPL_H_
