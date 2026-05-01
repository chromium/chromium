// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_INJECTOR_H_
#define REMOTING_HOST_AUDIO_INJECTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace remoting {

class AudioPacket;

// A class for injecting audio packets into a virtual audio input device.
class AudioInjector {
 public:
  // Returns true if the current platform supports audio injection.
  // Note: For multi-process host, returning true only means that the platform
  // supports audio injection. The AudioInjector class itself may only
  // work in the desktop process due to user isolation.
  static bool IsSupported();

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called whenever the number of active consumers changes from 0 to >=1, or
    // from >=1 to 0. This may be used to, e.g., only enable the microphone on
    // the client side if there is an app that is hooked up to the virtual input
    // device.
    virtual void OnAudioInjectorConsumersChanged(bool has_consumers) = 0;
  };

  virtual ~AudioInjector() = default;

  // Starts a virtual audio input device for injecting audio packets.
  virtual bool Start(base::WeakPtr<Delegate> delegate) = 0;

  // Injects an audio packet into the virtual audio input device. Must be called
  // after Start().
  virtual void InjectAudioPacket(std::unique_ptr<AudioPacket> packet) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_INJECTOR_H_
