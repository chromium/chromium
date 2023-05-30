// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NET_LOG_PROXY_SINK_H_
#define SERVICES_NETWORK_NET_LOG_PROXY_SINK_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/log/net_log.h"
#include "services/network/public/mojom/net_log.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace network {

// Implementation of NetLogProxySink mojo interface which receives external
// NetLog events and injects them into the NetLog. Also notifies attached
// sources when the NetLog capture mode changes.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetLogProxySink
    : public net::NetLog::ThreadSafeCaptureModeObserver,
      public network::mojom::NetLogProxySink {
 public:
  NetLogProxySink();
  ~NetLogProxySink() override;
  NetLogProxySink(const NetLogProxySink&) = delete;
  NetLogProxySink& operator=(const NetLogProxySink&) = delete;

  // Attaches a source of external NetLog events. The source is automatically
  // removed if the mojo pipes are closed.
  void AttachSource(mojo::PendingRemote<network::mojom::NetLogProxySource>
                        proxy_source_remote,
                    mojo::PendingReceiver<network::mojom::NetLogProxySink>
                        proxy_sink_receiver);

  // net::NetLog::ThreadSafeCaptureModeObserver:
  // Notifies attached sources that the capture mode has changed.
  void OnCaptureModeUpdated(net::NetLogCaptureModeSet modes) override;

  // mojom::NetLogProxySink:
  void AddEntry(uint32_t type,
                const net::NetLogSource& net_log_source,
                net::NetLogEventPhase phase,
                base::TimeTicks time,
                base::Value::Dict params) override;

 private:
  mojo::RemoteSet<network::mojom::NetLogProxySource> proxy_source_remotes_;
  mojo::ReceiverSet<network::mojom::NetLogProxySink> proxy_sink_receivers_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // A WeakPtr to |this|, which is used when posting tasks from the
  // NetLog::ThreadSafeCaptureModeObserver to |task_runner_|. This single
  // WeakPtr instance is used for all tasks as the ThreadSafeObserver may call
  // on any thread, so the weak_factory_ cannot be accessed safely from those
  // threads.
  base::WeakPtr<NetLogProxySink> weak_this_;
  base::WeakPtrFactory<NetLogProxySink> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NET_LOG_PROXY_SINK_H_
