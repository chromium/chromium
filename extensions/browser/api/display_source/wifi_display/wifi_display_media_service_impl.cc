// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/display_source/wifi_display/wifi_display_media_service_impl.h"

#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"

using content::BrowserThread;

namespace extensions {

class WiFiDisplayMediaServiceImpl::PacketIOBuffer : public net::IOBuffer {
 public:
  explicit PacketIOBuffer(std::vector<uint8_t> array);

  int size() const { return packet_data_.size(); }

 private:
  ~PacketIOBuffer() override;

  std::vector<uint8_t> packet_data_;
};

WiFiDisplayMediaServiceImpl::PacketIOBuffer::PacketIOBuffer(
    std::vector<uint8_t> array) {
  array.Swap(&packet_data_);
  data_ = reinterpret_cast<char*>(packet_data_.data());
}

WiFiDisplayMediaServiceImpl::PacketIOBuffer::~PacketIOBuffer() {
  data_ = nullptr;
}

// static
void WiFiDisplayMediaServiceImpl::Create(
    mojom::WiFiDisplayMediaServiceRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto* impl = new WiFiDisplayMediaServiceImpl();
  impl->binding_ =
      mojo::MakeStrongBinding(base::WrapUnique(impl), std::move(request));
}

// static
void WiFiDisplayMediaServiceImpl::BindToRequest(
    mojom::WiFiDisplayMediaServiceRequest request,
    content::RenderFrameHost* render_frame_host) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::Bind(WiFiDisplayMediaServiceImpl::Create,
                                      base::Passed(std::move(request))));
}

WiFiDisplayMediaServiceImpl::WiFiDisplayMediaServiceImpl()
    : last_send_code_(net::OK), weak_factory_(this) {}

WiFiDisplayMediaServiceImpl::~WiFiDisplayMediaServiceImpl() {}

void WiFiDisplayMediaServiceImpl::SetDesinationPoint(
    const std::string& ip_address,
    int32_t port,
    const SetDesinationPointCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(std::string(ip_address))) {
    DVLOG(1) << "Failed to parse IP address from " << ip_address;
    callback.Run(false);
    return;
  }
  net::IPEndPoint end_point(address, static_cast<uint16_t>(port));

  rtp_socket_.reset(new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                                       net::RandIntCallback(), nullptr,
                                       net::NetLogSource()));
  if (rtp_socket_->Open(end_point.GetFamily()) != net::OK ||
      rtp_socket_->Connect(end_point) != net::OK) {
    DVLOG(1) << "Could not connect to " << end_point.ToString();
    callback.Run(false);
    rtp_socket_.reset();
    return;
  }
  callback.Run(true);
}

void WiFiDisplayMediaServiceImpl::SendMediaPacket(
    mojom::WiFiDisplayMediaPacketPtr packet) {
  DCHECK(rtp_socket_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!packet) {
    DVLOG(1) << "Packet missing, skipping.";
    return;
  }

  std::vector<uint8_t>* packet_data = &packet->data;

  if (packet_data->size() >> 15) {
    DVLOG(1) << "Packet size limit is exceeded, skipping.";
    return;
  }

  if (last_send_code_ == net::ERR_IO_PENDING) {
    VLOG(1) << "Cannot send because of pending IO, skipping";
    return;
  }

  // Create, queue and send a write buffer.
  scoped_refptr<PacketIOBuffer> write_buffer =
      new PacketIOBuffer(std::move(*packet_data));
  write_buffers_.push(std::move(write_buffer));

  Send();
}

void WiFiDisplayMediaServiceImpl::Send() {
  DCHECK(!write_buffers_.empty());
  last_send_code_ = rtp_socket_->Write(
      write_buffers_.front().get(), write_buffers_.front()->size(),
      base::Bind(&WiFiDisplayMediaServiceImpl::OnSent,
                 weak_factory_.GetWeakPtr()));
  if (last_send_code_ != net::ERR_IO_PENDING)
    OnSent(last_send_code_);
}

void WiFiDisplayMediaServiceImpl::OnSent(int code) {
  last_send_code_ = code;
  if (code < 0) {
    VLOG(1) << "Unrepairable UDP socket error.";
    binding_->Close();
    return;
  }
  DCHECK(!write_buffers_.empty());
  write_buffers_.pop();
  if (!write_buffers_.empty())
    Send();
}

}  // namespace extensions
