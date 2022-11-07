// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_AUDIO_CAPTURER_H_
#define REMOTING_HOST_IPC_AUDIO_CAPTURER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/audio_capturer.h"

namespace remoting {

class AudioPacket;
class DesktopSessionProxy;

class IpcAudioCapturer : public AudioCapturer {
 public:
  explicit IpcAudioCapturer(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcAudioCapturer(const IpcAudioCapturer&) = delete;
  IpcAudioCapturer& operator=(const IpcAudioCapturer&) = delete;

  ~IpcAudioCapturer() override;

  // AudioCapturer interface.
  bool Start(const PacketCapturedCallback& callback) override;

  // Called by DesktopSessionProxy when an audio packet is received.
  void OnAudioPacket(std::unique_ptr<AudioPacket> packet);

 private:
  // Invoked when an audio packet was received.
  PacketCapturedCallback callback_;

  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcAudioCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_AUDIO_CAPTURER_H_
