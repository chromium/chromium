// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_AUDIO_INJECTOR_H_
#define REMOTING_HOST_IPC_AUDIO_INJECTOR_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/audio_injector.h"

namespace remoting {

class DesktopSessionProxy;
class IpcFifoBufferReader;

// Routes AudioInjector calls through the IPC channel to the
// DesktopSessionAgent running in the desktop integration process.
class IpcAudioInjector : public AudioInjector {
 public:
  IpcAudioInjector(scoped_refptr<DesktopSessionProxy> desktop_session_proxy,
                   std::unique_ptr<IpcFifoBufferReader> audio_reader);

  IpcAudioInjector(const IpcAudioInjector&) = delete;
  IpcAudioInjector& operator=(const IpcAudioInjector&) = delete;

  ~IpcAudioInjector() override;

  // AudioInjector implementation.
  bool Start(base::WeakPtr<Delegate> delegate) override;
  void SetSampleInfo(const protocol::AudioSampleInfo& info,
                     base::OnceClosure done) override;

  base::WeakPtr<AudioInjector> GetWeakPtr() override;

 private:
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;
  std::unique_ptr<IpcFifoBufferReader> audio_reader_;

  base::WeakPtrFactory<IpcAudioInjector> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_AUDIO_INJECTOR_H_
