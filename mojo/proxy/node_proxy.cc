// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/proxy/node_proxy.h"

#include <utility>

#include "base/check.h"
#include "base/synchronization/lock.h"
#include "mojo/proxy/portal_proxy.h"

namespace mojo_proxy {

NodeProxy::NodeProxy(const IpczAPI& ipcz, base::WaitableEvent& dead_event)
    : ipcz_(ipcz), dead_event_(dead_event) {}

NodeProxy::~NodeProxy() = default;

void NodeProxy::AddPortalProxy(mojo::core::ScopedIpczHandle portal,
                               mojo::ScopedMessagePipeHandle pipe) {
  auto proxy = std::make_unique<PortalProxy>(ipcz_, *this, std::move(portal),
                                             std::move(pipe));
  PortalProxy* proxy_ptr = proxy.get();
  {
    base::AutoLock lock(lock_);
    auto [it, inserted] = portal_proxies_.insert(std::move(proxy));
    CHECK(inserted);
  }

  // Safe because there's only one thread (the IO thread) operating on this
  // NodeProxy. Note that Start() may destroy `proxy_ptr`.
  proxy_ptr->Start();
}

void NodeProxy::RemovePortalProxy(PortalProxy* proxy) {
  bool dead;
  std::unique_ptr<PortalProxy> doomed_proxy;
  {
    base::AutoLock lock(lock_);
    auto it = portal_proxies_.find(proxy);
    if (it == portal_proxies_.end()) {
      return;
    }

    doomed_proxy = std::move(portal_proxies_.extract(it).value());

    // Once the proxy set is empty, it cannot become non-empty again.
    dead = portal_proxies_.empty();
  }

  // SUBTLE: It's important that we don't hold the lock while destroying a
  // PortalProxy because a portal or pipe's destruction may cause other
  // proxies to be created or torn down, reentering the NodeProxy.
  doomed_proxy.reset();

  if (dead) {
    // Signaling the death event is the last thing we do on last proxy removal,
    // effectively ensuring that no other NodeProxy state will be touched on the
    // IO thread once we signal.
    dead_event_->Signal();
  }
}

}  // namespace mojo_proxy
