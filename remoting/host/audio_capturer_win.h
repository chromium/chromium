// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_WIN_H_
#define REMOTING_HOST_AUDIO_CAPTURER_WIN_H_

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <memory>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_co_mem.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/win/audio_volume_filter_win.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

class DefaultAudioDeviceChangeDetector;

// An AudioCapturer implementation for Windows by using Windows Audio Session
// API, a.k.a. WASAPI. It supports up to 8 channels, but treats all layouts as
// a most commonly used one. E.g. 3.1 and surround layouts will both be marked
// as surround layout.
class AudioCapturerWin : public AudioCapturer {
 public:
  AudioCapturerWin();

  AudioCapturerWin(const AudioCapturerWin&) = delete;
  AudioCapturerWin& operator=(const AudioCapturerWin&) = delete;

  ~AudioCapturerWin() override;

  // AudioCapturer interface.
  bool Start(const PacketCapturedCallback& callback) override;

 private:
  // Executes Deinitialize() and Initialize(). If Initialize() function call
  // returns false, Deinitialize() will be called again to ensure we will
  // initialize COM components again.
  bool ResetAndInitialize();

  // Resets all COM components to nullptr, so is_initialized() will return
  // false.
  void Deinitialize();

  // Initializes default audio device related components. These components must
  // be recreated once the default audio device changed. Returns false if
  // initialization failed.
  bool Initialize();

  // Whether all components are correctly initialized. If last
  // Initialize() function call failed, this function will return false.
  // Otherwise this function will return true.
  bool is_initialized() const;

  // Receives all packets from the audio capture endpoint buffer and pushes them
  // to the network.
  void DoCapture();

  PacketCapturedCallback callback_;

  AudioPacket::SamplingRate sampling_rate_;

  std::unique_ptr<base::RepeatingTimer> capture_timer_;
  base::TimeDelta audio_device_period_;

  AudioVolumeFilterWin volume_filter_;

  base::win::ScopedCoMem<WAVEFORMATEX> wave_format_ex_;
  Microsoft::WRL::ComPtr<IAudioCaptureClient> audio_capture_client_;
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  Microsoft::WRL::ComPtr<IMMDevice> mm_device_;

  std::unique_ptr<DefaultAudioDeviceChangeDetector> default_device_detector_;

  HRESULT last_capture_error_;

  base::ThreadChecker thread_checker_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_WIN_H_
