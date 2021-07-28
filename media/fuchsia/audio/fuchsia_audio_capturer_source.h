// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_AUDIO_FUCHSIA_AUDIO_CAPTURER_SOURCE_H_
#define MEDIA_FUCHSIA_AUDIO_FUCHSIA_AUDIO_CAPTURER_SOURCE_H_

#include <fuchsia/media/cpp/fidl.h>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT FuchsiaAudioCapturerSource final
    : public AudioCapturerSource {
 public:
  explicit FuchsiaAudioCapturerSource(
      fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer_handle);

  FuchsiaAudioCapturerSource(const FuchsiaAudioCapturerSource&) = delete;
  FuchsiaAudioCapturerSource& operator=(const FuchsiaAudioCapturerSource&) =
      delete;

  // AudioCaptureSource implementation.
  void Initialize(const AudioParameters& params,
                  CaptureCallback* callback) override;
  void Start() override;
  void Stop() override;
  void SetVolume(double volume) override;
  void SetAutomaticGainControl(bool enable) override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  ~FuchsiaAudioCapturerSource() override;

  void NotifyCaptureError(const std::string& error);
  void NotifyCaptureStarted();
  void OnPacketCaptured(fuchsia::media::StreamPacket packet);
  void ReportError(const std::string& message);

  // AudioCapturer handle before it's bound to |capturer_| in Initialize().
  // Normally FuchsiaAudioCapturerSource is created on a thread different from
  // the main thread on which it is used, so |capturer_| cannot be initialized
  // in the constructor.
  fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer_handle_;

  fuchsia::media::AudioCapturerPtr capturer_;

  AudioParameters params_;
  CaptureCallback* callback_ = nullptr;

  // Shared VMO mapped to the current address space.
  uint8_t* capture_buffer_ = nullptr;
  size_t capture_buffer_size_ = 0;

  // Indicates that async capture mode has been activated for |capturer_|, i.e.
  // StartAsyncCapture() has been called.
  bool is_capturer_started_ = false;

  // Set to true between Start() and Stop().
  bool is_active_ = false;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<FuchsiaAudioCapturerSource> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_AUDIO_FUCHSIA_AUDIO_CAPTURER_SOURCE_H_
