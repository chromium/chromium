// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_INJECTOR_H_
#define REMOTING_HOST_AUDIO_INJECTOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace remoting {

class FifoBufferReader;

namespace protocol {
struct AudioSampleInfo;
}  // namespace protocol

// A class for injecting audio streams from a FIFO buffer into a virtual audio
// input device.
class AudioInjector {
 public:
  // Returns true if the current platform supports audio injection.
  // Note: For multi-process host, returning true only means that the platform
  // supports audio injection. The AudioInjector class itself may only
  // work in the desktop process due to user isolation.
  static bool IsSupported();
  static std::unique_ptr<AudioInjector> Create(
      std::unique_ptr<FifoBufferReader> audio_reader);

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called whenever the number of active consumers changes from 0 to >=1, or
    // from >=1 to 0. This may be used to, e.g., only enable the microphone on
    // the client side if there is an app that is hooked up to the virtual input
    // device.
    virtual void OnAudioInjectorConsumersChanged(bool has_consumers) = 0;
  };

  virtual ~AudioInjector();

  // Starts pumping audio from the reader into the virtual audio input device.
  virtual bool Start(base::WeakPtr<Delegate> delegate) = 0;

  // Sets the sample rate and channels for the injected audio stream. When this
  // method is called, the writer will stop writing to the buffer until `done`
  // is called, then it will start writing samples in the new format.
  virtual void SetSampleInfo(const protocol::AudioSampleInfo& info,
                             base::OnceClosure done) = 0;

  virtual base::WeakPtr<AudioInjector> GetWeakPtr() = 0;

 protected:
  AudioInjector();
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_INJECTOR_H_
