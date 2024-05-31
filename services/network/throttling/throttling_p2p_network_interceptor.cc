// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_p2p_network_interceptor.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "services/network/p2p/socket_udp.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/throttling/network_conditions.h"
#include "third_party/webrtc/api/units/data_rate.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace {
constexpr int kBitsPerByte = 8;
}

namespace network {
ThrottlingP2PNetworkInterceptor::StoredSendPacket::StoredSendPacket(
    raw_ptr<P2PSocketUdp> socket,
    P2PPendingPacket packet)
    : socket(socket), packet(packet) {}
ThrottlingP2PNetworkInterceptor::StoredSendPacket::~StoredSendPacket() =
    default;

ThrottlingP2PNetworkInterceptor::StoredReceivePacket::StoredReceivePacket(
    raw_ptr<P2PSocketUdp> socket,
    mojom::P2PReceivedPacketPtr packet,
    scoped_refptr<net::IOBuffer> buffer)
    : socket(socket), packet(std::move(packet)), buffer(std::move(buffer)) {}
ThrottlingP2PNetworkInterceptor::StoredReceivePacket::~StoredReceivePacket() =
    default;

ThrottlingP2PNetworkInterceptor::ThrottlingP2PNetworkInterceptor()
    : send_network_(webrtc::SimulatedNetwork::Config{}),
      send_packet_id_(0),
      receive_network_(webrtc::SimulatedNetwork::Config{}),
      receive_packet_id_(0) {}

ThrottlingP2PNetworkInterceptor::~ThrottlingP2PNetworkInterceptor() {
  send_timer_.Stop();
  receive_timer_.Stop();

  // The interceptor is being destroyed while we still have packets to send for
  // some sockets, we drop all the packets.
  for (auto& socket : sockets_) {
    socket->DisconnectInterceptor();
  }
}

void ThrottlingP2PNetworkInterceptor::RegisterSocket(P2PSocketUdp* socket) {
  sockets_.push_back(socket);
}

void ThrottlingP2PNetworkInterceptor::UnregisterSocket(P2PSocketUdp* socket) {
  VLOG(1) << "Removing socket 0x" << socket << " from interceptor";
  // The socket is being closed, we don't need to send the actual packets,
  // just clean up the local state
  std::erase_if(send_packets_, [&socket](auto& kv) -> bool {
    return kv.second.socket == socket;
  });
  std::erase_if(receive_packets_, [&socket](auto& kv) -> bool {
    return kv.second.socket == socket;
  });
  std::erase(sockets_, socket);
}

void ThrottlingP2PNetworkInterceptor::UpdateConditions(
    const NetworkConditions& conditions) {
  conditions_ = std::make_unique<NetworkConditions>(conditions);

  VLOG(1) << "UpdateConditions: upload: "
          << static_cast<uint64_t>(conditions.upload_throughput() *
                                   kBitsPerByte)
          << "kbps, download: "
          << static_cast<uint64_t>(conditions.download_throughput() *
                                   kBitsPerByte)
          << "kbps, latency: " << static_cast<uint64_t>(conditions.latency())
          << "ms, packet drop: " << conditions.packet_loss()
          << "%, packet queue: " << conditions.packet_queue_length()
          << ", packet reordering: " << conditions.packet_reordering();

  webrtc::SimulatedNetwork::Config send_config;
  send_config.link_capacity =
      conditions.upload_throughput() <= 0
          ? webrtc::DataRate::Infinity()
          : webrtc::DataRate::BytesPerSec(conditions.upload_throughput());
  send_config.queue_delay_ms = conditions.latency();
  send_config.allow_reordering = conditions.packet_reordering();
  send_config.loss_percent = conditions.packet_loss();
  send_config.queue_length_packets = conditions.packet_queue_length();
  send_network_.SetConfig(send_config);

  webrtc::SimulatedNetwork::Config receive_config;
  receive_config.link_capacity =
      conditions.download_throughput() <= 0
          ? webrtc::DataRate::Infinity()
          : webrtc::DataRate::BytesPerSec(conditions.download_throughput());
  receive_config.queue_delay_ms = conditions.latency();
  receive_config.allow_reordering = conditions.packet_reordering();
  receive_config.loss_percent = conditions.packet_loss();
  receive_network_.SetConfig(receive_config);
}

void ThrottlingP2PNetworkInterceptor::EnqueueSend(P2PPendingPacket packet,
                                                  P2PSocketUdp* socket) {
  int64_t now = rtc::TimeMicros();
  socket->SendCompletionFromInterceptor(P2PSendPacketMetrics{
      packet.id, static_cast<int32_t>(packet.packet_options.packet_id),
      now / 1000});

  //  If we're offline, discard the packet
  if (conditions_->offline()) {
    VLOG(3) << "Offline, discarding packet id: " << packet.id;
    return;
  }

  uint64_t packet_id = send_packet_id_++;
  webrtc::PacketInFlightInfo packet_info(packet.size, now, packet_id);

  if (send_network_.EnqueuePacket(packet_info)) {
    send_packets_.try_emplace(packet_id, socket, std::move(packet));
    std::optional<int64_t> next_delivery_time =
        send_network_.NextDeliveryTimeUs();

    if (next_delivery_time) {
      send_timer_.Start(
          FROM_HERE,
          base::Microseconds(*next_delivery_time) - base::Microseconds(now),
          base::BindOnce(&ThrottlingP2PNetworkInterceptor::OnSendNetworkTimer,
                         base::Unretained(this)));
    }
  } else {
    VLOG(3) << "Dropping packet id: " << packet.id;
  }
}

void ThrottlingP2PNetworkInterceptor::OnSendNetworkTimer() {
  int64_t now = rtc::TimeMicros();
  std::vector<webrtc::PacketDeliveryInfo> packets =
      send_network_.DequeueDeliverablePackets(now);
  for (auto& packet : packets) {
    SendPacketMap::iterator packet_iterator =
        send_packets_.find(packet.packet_id);
    if (packet_iterator == send_packets_.end()) {
      // Socket and all associated packet data has been removed, ignore
      continue;
    }
    P2PSocketUdp* socket = packet_iterator->second.socket;

    // Check for dropped packets
    if (packet.receive_time_us != webrtc::PacketDeliveryInfo::kNotReceived) {
      socket->SendFromInterceptor(packet_iterator->second.packet);
    }
    send_packets_.erase(packet_iterator);
  }

  // Schedule the next delivery timer
  auto next_delivery_time = send_network_.NextDeliveryTimeUs();
  if (next_delivery_time) {
    send_timer_.Start(
        FROM_HERE,
        base::Microseconds(*next_delivery_time) - base::Microseconds(now),
        base::BindOnce(&ThrottlingP2PNetworkInterceptor::OnSendNetworkTimer,
                       base::Unretained(this)));
  }
}

void ThrottlingP2PNetworkInterceptor::EnqueueReceive(
    mojom::P2PReceivedPacketPtr packet,
    scoped_refptr<net::IOBuffer> buffer,
    P2PSocketUdp* socket) {
  //  If we're offline, discard the packet
  if (conditions_->offline()) {
    VLOG(3) << "Offline, discarding incoming packet";
    return;
  }

  uint64_t packet_id = receive_packet_id_++;
  int64_t now = rtc::TimeMicros();
  webrtc::PacketInFlightInfo packet_info(packet->data.size(), now, packet_id);

  if (receive_network_.EnqueuePacket(packet_info)) {
    receive_packets_.try_emplace(packet_id, socket, std::move(packet),
                                 std::move(buffer));
    std::optional<int64_t> next_delivery_time =
        receive_network_.NextDeliveryTimeUs();

    if (next_delivery_time) {
      receive_timer_.Start(
          FROM_HERE,
          base::Microseconds(*next_delivery_time) - base::Microseconds(now),
          base::BindOnce(
              &ThrottlingP2PNetworkInterceptor::OnReceiveNetworkTimer,
              base::Unretained(this)));
    }
  }
}

void ThrottlingP2PNetworkInterceptor::OnReceiveNetworkTimer() {
  int64_t now = rtc::TimeMicros();
  std::vector<webrtc::PacketDeliveryInfo> packets =
      receive_network_.DequeueDeliverablePackets(now);
  for (auto& packet : packets) {
    ReceivePacketMap::iterator packet_iterator =
        receive_packets_.find(packet.packet_id);
    if (packet_iterator == receive_packets_.end()) {
      // Socket and all associated packet data has been removed, ignore
      continue;
    }

    P2PSocketUdp* socket = packet_iterator->second.socket;

    // Check for dropped packets
    if (packet.receive_time_us != webrtc::PacketDeliveryInfo::kNotReceived) {
      packet_iterator->second.packet->timestamp =
          base::TimeTicks() + base::Microseconds(packet.receive_time_us);
      socket->ReceiveFromInterceptor(std::move(packet_iterator->second.packet),
                                     std::move(packet_iterator->second.buffer));
    }
    receive_packets_.erase(packet_iterator);
  }

  // Schedule the next delivery timer
  auto next_delivery_time = receive_network_.NextDeliveryTimeUs();
  if (next_delivery_time) {
    receive_timer_.Start(
        FROM_HERE,
        base::Microseconds(*next_delivery_time) - base::Microseconds(now),
        base::BindOnce(&ThrottlingP2PNetworkInterceptor::OnReceiveNetworkTimer,
                       base::Unretained(this)));
  }
}

}  // namespace network
