// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_proxy.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

QuicTransportProxy::QuicTransportProxy(
    Delegate* delegate,
    IceTransportProxy* ice_transport_proxy,
    std::unique_ptr<P2PQuicTransportFactory> quic_transport_factory,
    const P2PQuicTransportConfig& config)
    : host_(nullptr,
            base::OnTaskRunnerDeleter(ice_transport_proxy->host_thread())),
      delegate_(delegate),
      ice_transport_proxy_(ice_transport_proxy) {
  DCHECK(delegate_);
  DCHECK(ice_transport_proxy_);
  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread =
      ice_transport_proxy->proxy_thread();
  DCHECK(proxy_thread->BelongsToCurrentThread());
  // Wait to initialize the host until the weak_ptr_factory_ is initialized.
  // The QuicTransportHost is constructed on the proxy thread but should only be
  // interacted with via PostTask to the host thread. The OnTaskRunnerDeleter
  // (configured above) will ensure it gets deleted on the host thread.
  host_.reset(new QuicTransportHost(weak_ptr_factory_.GetWeakPtr(),
                                    std::move(quic_transport_factory)));
  // Connect to the IceTransportProxy. This gives us a reference to the
  // underlying IceTransportHost that should be connected by the
  // QuicTransportHost on the host thread. It is safe to post it unretained
  // since the IceTransportHost's ownership is determined by the
  // IceTransportProxy, and the IceTransportProxy is required to outlive this
  // object.
  IceTransportHost* ice_transport_host =
      ice_transport_proxy->ConnectConsumer(this);
  PostCrossThreadTask(
      *host_thread(), FROM_HERE,
      CrossThreadBindOnce(&QuicTransportHost::Initialize,
                          CrossThreadUnretained(host_.get()),
                          CrossThreadUnretained(ice_transport_host), config));
}

QuicTransportProxy::~QuicTransportProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ice_transport_proxy_->DisconnectConsumer(this);
  // Note: The QuicTransportHost will be deleted on the host thread.
}

scoped_refptr<base::SingleThreadTaskRunner> QuicTransportProxy::proxy_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ice_transport_proxy_->proxy_thread();
}

scoped_refptr<base::SingleThreadTaskRunner> QuicTransportProxy::host_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ice_transport_proxy_->host_thread();
}

void QuicTransportProxy::Start(P2PQuicTransport::StartConfig config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBindOnce(&QuicTransportHost::Start,
                                          CrossThreadUnretained(host_.get()),
                                          WTF::Passed(std::move(config))));
}

void QuicTransportProxy::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBindOnce(&QuicTransportHost::Stop,
                                          CrossThreadUnretained(host_.get())));
}

QuicStreamProxy* QuicTransportProxy::CreateStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto stream_proxy = std::make_unique<QuicStreamProxy>();
  auto stream_host = std::make_unique<QuicStreamHost>();
  stream_proxy->set_host(stream_host->AsWeakPtr());
  stream_host->set_proxy(stream_proxy->AsWeakPtr());

  stream_proxy->Initialize(this);

  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBindOnce(&QuicTransportHost::CreateStream,
                                          CrossThreadUnretained(host_.get()),
                                          WTF::Passed(std::move(stream_host))));

  QuicStreamProxy* stream_proxy_ptr = stream_proxy.get();
  stream_proxies_.insert(stream_proxy_ptr, std::move(stream_proxy));
  return stream_proxy_ptr;
}

void QuicTransportProxy::SendDatagram(Vector<uint8_t> datagram) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBindOnce(&QuicTransportHost::SendDatagram,
                                          CrossThreadUnretained(host_.get()),
                                          std::move(datagram)));
}

void QuicTransportProxy::GetStats(uint32_t request_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  PostCrossThreadTask(
      *host_thread(), FROM_HERE,
      CrossThreadBindOnce(&QuicTransportHost::GetStats,
                          CrossThreadUnretained(host_.get()), request_id));
}

void QuicTransportProxy::OnRemoveStream(
    QuicStreamProxy* stream_proxy_to_remove) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = stream_proxies_.find(stream_proxy_to_remove);
  DCHECK(it != stream_proxies_.end());
  stream_proxies_.erase(it);
}

void QuicTransportProxy::OnConnected(
    P2PQuicNegotiatedParams negotiated_params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnConnected(negotiated_params);
}

void QuicTransportProxy::OnRemoteStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stream_proxies_.clear();
  delegate_->OnRemoteStopped();
}

void QuicTransportProxy::OnConnectionFailed(const std::string& error_details,
                                            bool from_remote) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stream_proxies_.clear();
  delegate_->OnConnectionFailed(error_details, from_remote);
}

void QuicTransportProxy::OnStream(
    std::unique_ptr<QuicStreamProxy> stream_proxy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  stream_proxy->Initialize(this);

  QuicStreamProxy* stream_proxy_ptr = stream_proxy.get();
  stream_proxies_.insert(stream_proxy_ptr, std::move(stream_proxy));
  delegate_->OnStream(stream_proxy_ptr);
}

void QuicTransportProxy::OnStats(uint32_t request_id,
                                 const P2PQuicTransportStats& stats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegate_->OnStats(request_id, stats);
}

void QuicTransportProxy::OnDatagramSent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegate_->OnDatagramSent();
}

void QuicTransportProxy::OnDatagramReceived(Vector<uint8_t> datagram) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegate_->OnDatagramReceived(std::move(datagram));
}

}  // namespace blink
