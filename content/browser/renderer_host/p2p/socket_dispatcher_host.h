// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

namespace content {

// Responsible for P2P sockets. Lives on the UI thread.
class P2PSocketDispatcherHost
    : public network::mojom::P2PTrustedSocketManagerClient {
 public:
  explicit P2PSocketDispatcherHost(int render_process_id);
  ~P2PSocketDispatcherHost() override;

  // Starts the RTP packet header dumping.
  void StartRtpDump(
      bool incoming,
      bool outgoing,
      const RenderProcessHost::WebRtcRtpPacketCallback& packet_callback);

  // Stops the RTP packet header dumping.
  void StopRtpDump(bool incoming, bool outgoing);

  void BindRequest(network::mojom::P2PSocketManagerRequest request);

  base::WeakPtr<P2PSocketDispatcherHost> GetWeakPtr();

 private:
  // network::mojom::P2PTrustedSocketManagerClient overrides:
  void InvalidSocketPortRangeRequested() override;
  void DumpPacket(const std::vector<uint8_t>& packet_header,
                  uint64_t packet_length,
                  bool incoming) override;

  int render_process_id_;

  bool dump_incoming_rtp_packet_ = false;
  bool dump_outgoing_rtp_packet_ = false;
  RenderProcessHost::WebRtcRtpPacketCallback packet_callback_;

  mojo::Receiver<network::mojom::P2PTrustedSocketManagerClient> receiver_{this};
  mojo::Remote<network::mojom::P2PTrustedSocketManager> trusted_socket_manager_;

  network::mojom::P2PNetworkNotificationClientPtr network_notification_client_;

  base::WeakPtrFactory<P2PSocketDispatcherHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
