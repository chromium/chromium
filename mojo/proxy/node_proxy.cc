// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/proxy/node_proxy.h"

#include <utility>

#include "base/check.h"
#include "mojo/proxy/portal_proxy.h"

namespace mojo_proxy {

NodeProxy::NodeProxy(const IpczAPI& ipcz, base::OnceClosure dead_callback)
    : ipcz_(ipcz), dead_callback_(std::move(dead_callback)) {}

NodeProxy::~NodeProxy() = default;

void NodeProxy::AddPortalProxy(mojo::core::ScopedIpczHandle portal,
                               mojo::ScopedMessagePipeHandle pipe) {
  auto proxy = std::make_unique<PortalProxy>(ipcz_, *this, std::move(portal),
                                             std::move(pipe));
  PortalProxy* proxy_ptr = proxy.get();

  auto [it, inserted] = portal_proxies_.insert(std::move(proxy));
  CHECK(inserted);

  // Safe because there's only one thread operating on this NodeProxy.
  // Note that Start() may destroy `proxy_ptr`.
  proxy_ptr->Start();
}

void NodeProxy::RemovePortalProxy(PortalProxy* proxy) {
  bool dead;
  auto it = portal_proxies_.find(proxy);
  if (it == portal_proxies_.end()) {
    return;
  }

  // Once the proxy set is empty, it cannot become non-empty again.
  portal_proxies_.erase(it);
  dead = portal_proxies_.empty();

  if (dead) {
    // Signaling the death event is the last thing we do on last proxy removal,
    // effectively ensuring that no other NodeProxy state will be touched once
    // we signal.
    std::move(dead_callback_).Run();
  }
}

}  // namespace mojo_proxy
