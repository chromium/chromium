// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_WAVEIN_INPUT_WIN_H_
#define MEDIA_AUDIO_WIN_WAVEIN_INPUT_WIN_H_

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_parameters.h"

// Put this in the declarations for a class to be uncopyable and unassignable.


namespace media {

class AudioBus;
class AudioManagerWin;

class PCMWaveInAudioInputStream : public  AudioInputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object and |device_id| which
  // is provided by the operating system.
  PCMWaveInAudioInputStream(AudioManagerWin* manager,
                            const AudioParameters& params,
                            int num_buffers,
                            const std::string& device_id);
  ~PCMWaveInAudioInputStream() override;

  // Implementation of AudioInputStream.
  OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  // TODO(henrika): Add volume support using the Audio Mixer API.
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  enum State {
    kStateEmpty,      // Initial state.
    kStateReady,      // Device obtained and ready to record.
    kStateRecording,  // Recording audio.
    kStateStopping,   // Trying to stop, waiting for callback to finish.
    kStateStopped,    // Stopped. Device was reset.
    kStateClosed      // Device has been released.
  };

  // Allow unit tests to query the device ID.
  friend class AudioManagerTest;

  // Windows calls us back with the recorded audio data here. See msdn
  // documentation for 'waveInProc' for details about the parameters.
  static void CALLBACK WaveCallback(HWAVEIN hwi, UINT msg, DWORD_PTR instance,
                                    DWORD_PTR param1, DWORD_PTR param2);

  // If windows reports an error this function handles it and passes it to
  // the attached AudioInputCallback::OnError().
  void HandleError(MMRESULT error);

  // Allocates and prepares the memory that will be used for recording.
  void SetupBuffers();

  // Deallocates the memory allocated in SetupBuffers.
  void FreeBuffers();

  // Sends a buffer to the audio driver for recording.
  void QueueNextPacket(WAVEHDR* buffer);

  // Converts the stored device id string into an unsigned integer which
  // can be used by waveInOpen() to open the specified capture device.
  bool GetDeviceId(UINT* device_index);

  base::ThreadChecker thread_checker_;

  // Reader beware. Visual C has stronger guarantees on volatile vars than
  // most people expect. In fact, it has release semantics on write and
  // acquire semantics on reads. See the msdn documentation.
  volatile State state_;

  // The audio manager that created this input stream. We notify it when
  // we close so it can release its own resources.
  raw_ptr<AudioManagerWin> manager_;

  // We use the callback mostly to periodically give the recorded audio data.
  raw_ptr<AudioInputCallback> callback_;

  // The number of buffers of size |buffer_size_| each to use.
  const int num_buffers_;

  // The size in bytes of each audio buffer.
  uint32_t buffer_size_;

  // Channels, 1 or 2.
  const int channels_;

  // Contains the unique name of the selected endpoint device.
  // Note that AudioDeviceDescription::kDefaultDeviceId represents the default
  // device role and is not a valid ID as such.
  std::string device_id_;

  // Windows native structure to encode the format parameters.
  WAVEFORMATEX format_;

  // Handle to the instance of the wave device.
  HWAVEIN wavein_;

  // Pointer to the first allocated audio buffer. This object owns it.
  raw_ptr<WAVEHDR> buffer_;

  // An event that is signaled when the callback thread is ready to stop.
  base::win::ScopedHandle stopped_event_;

  // Lock used to avoid conflicts when Stop() is called during a callback.
  base::Lock lock_;

  // Extra audio bus used for storage of deinterleaved data for the OnData
  // callback.
  std::unique_ptr<media::AudioBus> audio_bus_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_WAVEIN_INPUT_WIN_H_