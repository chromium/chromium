// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_REMOTE_AUDIO_INPUT_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_REMOTE_AUDIO_INPUT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/remote_audio_input.h"

namespace remoting {

class AudioPacket;

// PipeWire implementation of a remote audio input. It creates a virtual
// audio source in PipeWire and feeds audio packets into it.
class PipewireRemoteAudioInput : public RemoteAudioInput {
 public:
  PipewireRemoteAudioInput();
  ~PipewireRemoteAudioInput() override;

  PipewireRemoteAudioInput(const PipewireRemoteAudioInput&) = delete;
  PipewireRemoteAudioInput& operator=(const PipewireRemoteAudioInput&) = delete;

  // RemoteAudioInput implementation.
  bool Start(base::WeakPtr<Delegate> delegate) override;
  void OnAudioPacket(std::unique_ptr<AudioPacket> packet) override;

 private:
  class Core;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<Core> core_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_REMOTE_AUDIO_INPUT_H_
