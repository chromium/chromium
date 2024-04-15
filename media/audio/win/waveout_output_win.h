// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_
#define MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_

#include <windows.h>

#include <mmreg.h>
#include <mmsystem.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerWin;

// Implements PCM audio output support for Windows using the WaveXXX API.
// While not as nice as the DirectSound-based API, it should work in all target
// operating systems regardless or DirectX version installed. It is known that
// in some machines WaveXXX based audio is better while in others DirectSound
// is better.
//
// Important: the OnXXXX functions in AudioSourceCallback are called by more
// than one thread so it is important to have some form of synchronization if
// you are keeping state in it.
class PCMWaveOutAudioOutputStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the the
  // audio manager who is creating this object and |device_id| which is provided
  // by the operating system.
  PCMWaveOutAudioOutputStream(AudioManagerWin* manager,
                              const AudioParameters& params,
                              int num_buffers,
                              UINT device_id);

  PCMWaveOutAudioOutputStream(const PCMWaveOutAudioOutputStream&) = delete;
  PCMWaveOutAudioOutputStream& operator=(const PCMWaveOutAudioOutputStream&) =
      delete;

  ~PCMWaveOutAudioOutputStream() override;

  // Implementation of AudioOutputStream.
  bool Open() override;
  void Close() override;
  void Flush() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

  // Sends a buffer to the audio driver for playback.
  void QueueNextPacket(WAVEHDR* buffer);

 private:
  enum State {
    PCMA_BRAND_NEW,    // Initial state.
    PCMA_READY,        // Device obtained and ready to play.
    PCMA_PLAYING,      // Playing audio.
    PCMA_STOPPING,     // Audio is stopping, do not "feed" data to Windows.
    PCMA_CLOSED        // Device has been released.
  };

  // Returns pointer to the n-th buffer.
  inline WAVEHDR* GetBuffer(int n) const;

  // Size of one buffer in bytes, rounded up if necessary.
  inline size_t BufferSize() const;

  // Windows calls us back asking for more data when buffer_event_ signalled.
  // See MSDN for help on RegisterWaitForSingleObject() and waveOutOpen().
  static void NTAPI BufferCallback(PVOID lpParameter, BOOLEAN timer_fired);

  // If windows reports an error this function handles it and passes it to
  // the attached AudioSourceCallback::OnError(ErrorType type).
  void HandleError(MMRESULT error);

  // Allocates and prepares the memory that will be used for playback.
  void SetupBuffers();

  // Deallocates the memory allocated in SetupBuffers.
  void FreeBuffers();

  // Reader beware. Visual C has stronger guarantees on volatile vars than
  // most people expect. In fact, it has release semantics on write and
  // acquire semantics on reads. See the msdn documentation.
  volatile State state_;

  // The audio manager that created this output stream. We notify it when
  // we close so it can release its own resources.
  raw_ptr<AudioManagerWin> manager_;

  // We use the callback mostly to periodically request more audio data.
  raw_ptr<AudioSourceCallback> callback_;

  // The number of buffers of size |buffer_size_| each to use.
  const int num_buffers_;

  // The size in bytes of each audio buffer, we usually have two of these.
  uint32_t buffer_size_;

  // Volume level from 0 to 1.
  float volume_;

  // Channels from 0 to 8.
  const int channels_;

  // Number of bytes yet to be played in the hardware buffer.
  uint32_t pending_bytes_;

  // The id assigned by the operating system to the selected wave output
  // hardware device. Usually this is just -1 which means 'default device'.
  UINT device_id_;

  // Windows native structure to encode the format parameters.
  WAVEFORMATPCMEX format_;

  // Handle to the instance of the wave device.
  HWAVEOUT waveout_;

  // Handle to the buffer event.
  base::win::ScopedHandle buffer_event_;

  // Handle returned by RegisterWaitForSingleObject().
  HANDLE waiting_handle_;

  // Pointer to the allocated audio buffers, we allocate all buffers in one big
  // chunk. This object owns them.
  std::unique_ptr<char[]> buffers_;

  // Lock used to avoid the conflict when callbacks are called simultaneously.
  base::Lock lock_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  std::unique_ptr<AudioBus> audio_bus_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_WAVEOUT_OUTPUT_WIN_H_
