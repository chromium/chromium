// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_AUDIO_INJECTOR_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_AUDIO_INJECTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/audio_injector.h"

namespace remoting {
class FifoBufferReader;

class AudioPacket;

// PipeWire implementation of an audio injector. It creates a virtual
// audio source in PipeWire and feeds audio packets into it.
class PipewireAudioInjector : public AudioInjector {
 public:
  static bool IsSupported();
  static std::unique_ptr<PipewireAudioInjector> Create(
      std::unique_ptr<FifoBufferReader> audio_reader);

  PipewireAudioInjector();
  ~PipewireAudioInjector() override;

  PipewireAudioInjector(const PipewireAudioInjector&) = delete;
  PipewireAudioInjector& operator=(const PipewireAudioInjector&) = delete;

  // AudioInjector implementation.
  bool Start(base::WeakPtr<Delegate> delegate) override;
  void InjectAudioPacket(std::unique_ptr<AudioPacket> packet) override;
  base::WeakPtr<protocol::AudioStub> GetWeakPtr() override;

 private:
  class Core;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<Core> core_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PipewireAudioInjector> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_AUDIO_INJECTOR_H_
