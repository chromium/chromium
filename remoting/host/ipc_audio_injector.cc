// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_audio_injector.h"

#include <utility>

#include "remoting/base/ipc_fifo_buffer.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/protocol/audio_sample_info.h"

namespace remoting {

IpcAudioInjector::IpcAudioInjector(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy,
    std::unique_ptr<IpcFifoBufferReader> audio_reader)
    : desktop_session_proxy_(desktop_session_proxy),
      audio_reader_(std::move(audio_reader)) {}

IpcAudioInjector::~IpcAudioInjector() = default;

bool IpcAudioInjector::Start(base::WeakPtr<Delegate> delegate) {
  // Note that IpcAudioInjector doesn't need to keep track of the delegate
  // since the microphone control signal comes from the desktop process via Mojo
  // and is handled by DesktopSessionProxy.
  desktop_session_proxy_->StartAudioInjector(std::move(audio_reader_));
  return true;
}

void IpcAudioInjector::SetSampleInfo(const protocol::AudioSampleInfo& info,
                                     base::OnceClosure done) {
  desktop_session_proxy_->SetAudioInjectorSampleInfo(info, std::move(done));
}

base::WeakPtr<AudioInjector> IpcAudioInjector::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
