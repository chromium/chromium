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

// PipeWire implementation of an audio injector. It creates a virtual
// audio source in PipeWire and pumps audio streams from a FifoBufferReader into
// it.
class PipewireAudioInjector : public AudioInjector {
 public:
  static bool IsSupported();
  static std::unique_ptr<PipewireAudioInjector> Create(
      std::unique_ptr<FifoBufferReader> audio_reader);

  explicit PipewireAudioInjector(
      std::unique_ptr<FifoBufferReader> audio_reader);
  ~PipewireAudioInjector() override;

  PipewireAudioInjector(const PipewireAudioInjector&) = delete;
  PipewireAudioInjector& operator=(const PipewireAudioInjector&) = delete;

  // AudioInjector implementation.
  bool Start(base::WeakPtr<Delegate> delegate) override;
  void SetSampleInfo(const protocol::AudioSampleInfo& info,
                     base::OnceClosure done) override;
  base::WeakPtr<AudioInjector> GetWeakPtr() override;

 private:
  class Core;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<FifoBufferReader> audio_reader_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool format_ready_ = false;
  std::unique_ptr<Core> core_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PipewireAudioInjector> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_AUDIO_INJECTOR_H_
