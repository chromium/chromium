// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_listener.h"

namespace IPC {

bool Listener::OnMessageReceived(const Message& message) {
  return false;
}

std::string Listener::ToDebugString() {
  return "IPC::Listener";
}

}  // namespace IPC
