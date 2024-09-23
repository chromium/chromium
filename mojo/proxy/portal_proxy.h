// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PROXY_PORTAL_PROXY_H_
#define MOJO_PROXY_PORTAL_PROXY_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/trap.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo_proxy {

class NodeProxy;

// Maintains a proxy between a single ipcz portal and a corresponding legacy
// Mojo message pipe. Self-destructs when either end is disconnected.
class PortalProxy {
 public:
  PortalProxy(const raw_ref<const IpczAPI> ipcz,
              NodeProxy& node_proxy,
              mojo::core::ScopedIpczHandle portal,
              mojo::ScopedMessagePipeHandle pipe);
  ~PortalProxy();

  // Starts proxying. Until either the portal or the pipe is disconnected from
  // its peer, this will watch both objects for incoming messages and forward
  // them to the other.
  void Start();

 private:
  uintptr_t trap_context() const { return reinterpret_cast<uintptr_t>(this); }

  void Flush();
  void FlushAndWatchPortal();
  void FlushAndWatchPipe();
  mojo::core::ScopedIpczHandle TranslateMojoToIpczHandle(
      mojo::ScopedHandle handle);
  mojo::ScopedHandle TranslateIpczToMojoHandle(
      mojo::core::ScopedIpczHandle handle);

  static void OnIpczPortalActivity(const IpczTrapEvent* event) {
    reinterpret_cast<PortalProxy*>(event->context)
        ->HandlePortalActivity(event->condition_flags);
  }

  static void OnMojoPipeActivity(const MojoTrapEvent* event) {
    reinterpret_cast<PortalProxy*>(event->trigger_context)
        ->HandlePipeActivity(event->result);
  }

  void HandlePortalActivity(IpczTrapConditionFlags flags);
  void HandlePipeActivity(MojoResult result);
  void Die();

  bool in_flush_ = false;
  bool disconnected_ = false;
  bool watching_portal_ = false;
  bool watching_pipe_ = false;

  const raw_ref<const IpczAPI> ipcz_;
  raw_ref<NodeProxy> node_proxy_;
  const mojo::core::ScopedIpczHandle portal_;
  const mojo::ScopedMessagePipeHandle pipe_;
  mojo::ScopedTrapHandle pipe_trap_;
};

}  // namespace mojo_proxy

#endif  // MOJO_PROXY_PORTAL_PROXY_H_
