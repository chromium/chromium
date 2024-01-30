// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Low-latency audio capturing class utilizing audio input stream provided
// by a server process by use of an IPC interface.
//
// Relationship of classes:
//
//  AudioInputController                 AudioInputDevice
//           ^                                  ^
//           |                                  |
//           v                  IPC             v
// MojoAudioInputStream    <----------->  AudioInputIPC
//           ^                            (MojoAudioInputIPC)
//           |
//           v
// AudioInputDeviceManager
//
// Transportation of audio samples from the browser to the render process
// is done by using shared memory in combination with a SyncSocket.
// The AudioInputDevice user registers an AudioInputDevice::CaptureCallback by
// calling Initialize().  The callback will be called with recorded audio from
// the underlying audio layers.
// The session ID is used by the RenderFrameAudioInputStreamFactory to start
// the device referenced by this ID.
//
// State sequences:
//
// Start -> CreateStream ->
//       <- OnStreamCreated <-
//       -> RecordStream ->
//
// AudioInputDevice::Capture => low latency audio transport on audio thread =>
//
// Stop ->  CloseStream -> Close
//
// This class depends on the audio transport thread. That thread is responsible
// for calling the CaptureCallback and feeding it audio samples from the server
// side audio layer using a socket and shared memory.
//
// Implementation notes:
// - The user must call Stop() before deleting the class instance.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_DEVICE_H_
#define MEDIA_AUDIO_AUDIO_INPUT_DEVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "media/audio/alive_checker.h"
#include "media/audio/audio_device_thread.h"
#include "media/audio/audio_input_ipc.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AudioInputDevice : public AudioCapturerSource,
                                      public AudioInputIPCDelegate {
 public:
  enum Purpose : int8_t { kUserInput, kLoopback };
  enum class DeadStreamDetection : bool { kDisabled = false, kEnabled = true };

  AudioInputDevice() = delete;

  // NOTE: Clients must call Initialize() before using.
  // |enable_uma| controls logging of UMA stats. It is used to ensure that
  // stats are not logged for mirroring service streams.
  // |detect_dead_stream| controls the dead stream detection.
  AudioInputDevice(std::unique_ptr<AudioInputIPC> ipc,
                   Purpose purpose,
                   DeadStreamDetection detect_dead_stream);

  AudioInputDevice(const AudioInputDevice&) = delete;
  AudioInputDevice& operator=(const AudioInputDevice&) = delete;

  // AudioCapturerSource implementation.
  void Initialize(const AudioParameters& params,
                  CaptureCallback* callback) override;
  void Start() override;
  void Stop() override;
  void SetVolume(double volume) override;
  void SetAutomaticGainControl(bool enabled) override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  friend class base::RefCountedThreadSafe<AudioInputDevice>;

  // Our audio thread callback class.  See source file for details.
  class AudioThreadCallback;

  // Note: The ordering of members in this enum is critical to correct behavior!
  enum State {
    IPC_CLOSED,       // No more IPCs can take place.
    IDLE,             // Not started.
    CREATING_STREAM,  // Waiting for OnStreamCreated() to be called back.
    RECORDING,        // Receiving audio data.
  };

  // This enum is used for UMA, so the only allowed operation on this definition
  // is to add new states to the bottom, update kMaxValue, and update the
  // histogram "Media.Audio.Capture.StreamCallbackError2".
  enum Error {
    kNoError = 0,
    kErrorDuringCreation = 1,
    kErrorDuringCapture = 2,
    kMaxValue = kErrorDuringCapture
  };

  ~AudioInputDevice() override;

  // AudioInputIPCDelegate implementation.
  void OnStreamCreated(base::ReadOnlySharedMemoryRegion shared_memory_region,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool initially_muted) override;
  void OnError(AudioCapturerSource::ErrorCode code) override;
  void OnMuted(bool is_muted) override;
  void OnIPCClosed() override;

  // This is called by |alive_checker_| if it detects that the input stream is
  // dead.
  void DetectedDeadInputStream();

  AudioParameters audio_parameters_;

  const base::ThreadType thread_type_;

  const bool enable_uma_;

  raw_ptr<CaptureCallback> callback_;

  // A pointer to the IPC layer that takes care of sending requests over to
  // the stream implementation.  Only valid when state_ != IPC_CLOSED.
  std::unique_ptr<AudioInputIPC> ipc_;

  // Current state. See comments for State enum above.
  State state_;

  // For UMA stats. May only be accessed on the IO thread.
  Error had_error_ = kNoError;

  // Stores the Automatic Gain Control state. Default is false.
  bool agc_is_enabled_;

  // Controls the dead stream detection. Only the DSP hotword devices set this
  // to kDisabled to disable dead stream detection.
  const DeadStreamDetection detect_dead_stream_;

  // Checks regularly that the input stream is alive and notifies us if it
  // isn't by calling DetectedDeadInputStream(). Must outlive |audio_callback_|.
  std::unique_ptr<AliveChecker> alive_checker_;

  std::unique_ptr<AudioInputDevice::AudioThreadCallback> audio_callback_;
  std::unique_ptr<AudioDeviceThread> audio_thread_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Cache the output device used for AEC in case it's called before the stream
  // is created.
  std::optional<std::string> output_device_id_for_aec_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_DEVICE_H_
