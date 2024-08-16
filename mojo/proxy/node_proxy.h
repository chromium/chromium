// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PROXY_NODE_PROXY_H_
#define MOJO_PROXY_NODE_PROXY_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo_proxy {

class PortalProxy;

// A NodeProxy hosts a PortalProxy object for each proxied endpoint between a
// legacy Mojo Core node and a MojoIpcz network. As messages arrive from either
// side of these proxies, they're forwarded along; and if they contain other
// message pipes, new proxies are established for those new endpoints.
class NodeProxy {
 public:
  // Constructs a new NodeProxy which will call `dead_callback` once its last
  // proxy is removed.
  NodeProxy(const IpczAPI& ipcz, base::OnceClosure dead_callback);
  ~NodeProxy();

  // Registers a new PortalProxy to forward messages between `portal` and
  // `pipe`. The proxy is activated before this call returns.
  void AddPortalProxy(mojo::core::ScopedIpczHandle portal,
                      mojo::ScopedMessagePipeHandle pipe);

  // Removes `proxy` from this NodeProxy, effectively destroying it. Calls
  // `dead_callback_` if this was our last remaining portal proxy.
  void RemovePortalProxy(PortalProxy* proxy);

 private:
  const raw_ref<const IpczAPI> ipcz_;
  base::OnceClosure dead_callback_;
  std::set<std::unique_ptr<PortalProxy>, base::UniquePtrComparator>
      portal_proxies_;
};

}  // namespace mojo_proxy

#endif  // MOJO_PROXY_NODE_PROXY_H_
