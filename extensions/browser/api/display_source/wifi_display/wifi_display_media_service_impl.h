// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_SERVICE_IMPL_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_SERVICE_IMPL_H_

#include <memory>

#include "base/containers/queue.h"
#include "extensions/common/mojom/wifi_display_session_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/udp_socket.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

class WiFiDisplayMediaServiceImpl : public mojom::WiFiDisplayMediaService {
 public:
  ~WiFiDisplayMediaServiceImpl() override;
  static void BindToRequest(
      mojo::PendingReceiver<mojom::WiFiDisplayMediaService> receiver,
      content::RenderFrameHost* render_frame_host);

  void SetDestinationPoint(
      const net::IPEndPoint& ip_end_point,
      const SetDestinationPointCallback& callback) override;
  void SendMediaPacket(mojom::WiFiDisplayMediaPacketPtr packet) override;

 private:
  static void Create(
      mojo::PendingReceiver<mojom::WiFiDisplayMediaService> receiver);
  WiFiDisplayMediaServiceImpl();
  void Send();
  void OnSent(int code);
  std::unique_ptr<net::UDPSocket> rtp_socket_;
  class PacketIOBuffer;
  base::queue<scoped_refptr<PacketIOBuffer>> write_buffers_;
  int last_send_code_;
  mojo::SelfOwnedReceiverRef<mojom::WiFiDisplayMediaService> receiver_;
  base::WeakPtrFactory<WiFiDisplayMediaServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiFiDisplayMediaServiceImpl);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_SERVICE_IMPL_H_
