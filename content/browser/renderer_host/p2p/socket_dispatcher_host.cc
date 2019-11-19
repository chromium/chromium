// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/p2p_param_traits.h"

using content::BrowserMessageFilter;
using content::BrowserThread;

namespace content {

P2PSocketDispatcherHost::P2PSocketDispatcherHost(int render_process_id)
    : render_process_id_(render_process_id) {}

P2PSocketDispatcherHost::~P2PSocketDispatcherHost() {}

void P2PSocketDispatcherHost::StartRtpDump(
    bool incoming,
    bool outgoing,
    const RenderProcessHost::WebRtcRtpPacketCallback& packet_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if ((!dump_incoming_rtp_packet_ && incoming) ||
      (!dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = true;

    if (outgoing)
      dump_outgoing_rtp_packet_ = true;

    packet_callback_ = packet_callback;
    if (trusted_socket_manager_)
      trusted_socket_manager_->StartRtpDump(incoming, outgoing);
  }
}

void P2PSocketDispatcherHost::StopRtpDump(bool incoming, bool outgoing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if ((dump_incoming_rtp_packet_ && incoming) ||
      (dump_outgoing_rtp_packet_ && outgoing)) {
    if (incoming)
      dump_incoming_rtp_packet_ = false;

    if (outgoing)
      dump_outgoing_rtp_packet_ = false;

    if (!dump_incoming_rtp_packet_ && !dump_outgoing_rtp_packet_)
      packet_callback_.Reset();

    if (trusted_socket_manager_)
      trusted_socket_manager_->StopRtpDump(incoming, outgoing);
  }
}

void P2PSocketDispatcherHost::BindRequest(
    network::mojom::P2PSocketManagerRequest request) {
  auto* rph = RenderProcessHostImpl::FromID(render_process_id_);
  if (!rph)
    return;

  // In case the renderer was connected previously but the network process
  // crashed.
  receiver_.reset();
  auto trusted_socket_manager_client = receiver_.BindNewPipeAndPassRemote();

  trusted_socket_manager_.reset();
  rph->GetStoragePartition()->GetNetworkContext()->CreateP2PSocketManager(
      std::move(trusted_socket_manager_client),
      trusted_socket_manager_.BindNewPipeAndPassReceiver(), std::move(request));
  if (dump_incoming_rtp_packet_ || dump_outgoing_rtp_packet_) {
    trusted_socket_manager_->StartRtpDump(dump_incoming_rtp_packet_,
                                          dump_outgoing_rtp_packet_);
  }
}

base::WeakPtr<P2PSocketDispatcherHost> P2PSocketDispatcherHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void P2PSocketDispatcherHost::InvalidSocketPortRangeRequested() {
  bad_message::ReceivedBadMessage(render_process_id_,
                                  bad_message::SDH_INVALID_PORT_RANGE);
}

void P2PSocketDispatcherHost::DumpPacket(
    const std::vector<uint8_t>& packet_header,
    uint64_t packet_length,
    bool incoming) {
  if (!packet_callback_)
    return;

  std::unique_ptr<uint8_t[]> header_buffer(new uint8_t[packet_header.size()]);
  memcpy(header_buffer.get(), &packet_header[0], packet_header.size());

  packet_callback_.Run(std::move(header_buffer),
                       static_cast<size_t>(packet_header.size()),
                       static_cast<size_t>(packet_length), incoming);
}

}  // namespace content
