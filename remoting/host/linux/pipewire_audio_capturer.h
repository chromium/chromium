// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_AUDIO_CAPTURER_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_AUDIO_CAPTURER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/audio_capturer.h"

namespace remoting {

// PipeWire implementation of AudioCapturer interface which captures audio by
// creating a virtual audio sink in PipeWire and reading samples from it.
// By default, each user will have their own PipeWire server, connectable via
// $XDG_RUNTIME_DIR/pipewire-0.
class PipewireAudioCapturer : public AudioCapturer {
 public:
  PipewireAudioCapturer();
  ~PipewireAudioCapturer() override;

  PipewireAudioCapturer(const PipewireAudioCapturer&) = delete;
  PipewireAudioCapturer& operator=(const PipewireAudioCapturer&) = delete;

  static std::unique_ptr<PipewireAudioCapturer> Create();
  static bool IsSupported();

  // AudioCapturer implementation.
  bool Start(const PacketCapturedCallback& callback) override;

 private:
  class Core;

  void OnPacketCaptured(const PacketCapturedCallback& callback,
                        std::unique_ptr<AudioPacket> packet);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<Core> core_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PipewireAudioCapturer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_AUDIO_CAPTURER_H_
