// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_AUDIO_INPUT_H_
#define REMOTING_HOST_REMOTE_AUDIO_INPUT_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace remoting {

class AudioPacket;

// A class for injecting audio packets into a virtual audio input device.
class RemoteAudioInput {
 public:
  // Returns true if the current platform supports remote audio input.
  // Note: For multi-process host, returning true only means that the platform
  // supports remote audio input. The RemoteAudioInput class itself may only
  // work in the desktop process due to user isolation.
  static bool IsSupported();

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called whenever the number of active consumers changes from 0 to >=1, or
    // from >=1 to 0.
    virtual void OnActiveConsumersChanged(bool has_consumers) = 0;
  };

  virtual ~RemoteAudioInput() = default;

  // Starts a virtual audio input device for injecting audio packets.
  virtual bool Start(base::WeakPtr<Delegate> delegate) = 0;

  // Injects an audio packet into the virtual audio input device. Must be called
  // after Start().
  virtual void OnAudioPacket(std::unique_ptr<AudioPacket> packet) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_AUDIO_INPUT_H_
