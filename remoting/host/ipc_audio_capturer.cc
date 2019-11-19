// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_audio_capturer.h"

#include <utility>

#include "remoting/host/desktop_session_proxy.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

IpcAudioCapturer::IpcAudioCapturer(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {}

IpcAudioCapturer::~IpcAudioCapturer() = default;

bool IpcAudioCapturer::Start(const PacketCapturedCallback& callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = callback;
  desktop_session_proxy_->SetAudioCapturer(weak_factory_.GetWeakPtr());
  return true;
}

void IpcAudioCapturer::OnAudioPacket(std::unique_ptr<AudioPacket> packet) {
  callback_.Run(std::move(packet));
}

}  // namespace remoting
