// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_proxy.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

QuicTransportProxy::QuicTransportProxy(
    Delegate* delegate,
    IceTransportProxy* ice_transport_proxy,
    quic::Perspective perspective,
    const std::vector<rtc::scoped_refptr<rtc::RTCCertificate>>& certificates,
    std::unique_ptr<P2PQuicTransportFactory> quic_transport_factory)
    : host_(nullptr,
            base::OnTaskRunnerDeleter(ice_transport_proxy->host_thread())),
      delegate_(delegate),
      ice_transport_proxy_(ice_transport_proxy),
      weak_ptr_factory_(this) {
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
  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBind(&QuicTransportHost::Initialize,
                                      CrossThreadUnretained(host_.get()),
                                      CrossThreadUnretained(ice_transport_host),
                                      perspective, certificates));
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

void QuicTransportProxy::Start(
    std::vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *host_thread(), FROM_HERE,
      CrossThreadBind(&QuicTransportHost::Start,
                      CrossThreadUnretained(host_.get()),
                      WTF::Passed(std::move(remote_fingerprints))));
}

void QuicTransportProxy::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBind(&QuicTransportHost::Stop,
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
                      CrossThreadBind(&QuicTransportHost::CreateStream,
                                      CrossThreadUnretained(host_.get()),
                                      WTF::Passed(std::move(stream_host))));

  QuicStreamProxy* stream_proxy_ptr = stream_proxy.get();
  stream_proxies_.insert(
      std::make_pair(stream_proxy_ptr, std::move(stream_proxy)));
  return stream_proxy_ptr;
}

void QuicTransportProxy::OnRemoveStream(
    QuicStreamProxy* stream_proxy_to_remove) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = stream_proxies_.find(stream_proxy_to_remove);
  DCHECK(it != stream_proxies_.end());
  stream_proxies_.erase(it);
}

void QuicTransportProxy::OnConnected() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnConnected();
}

void QuicTransportProxy::OnRemoteStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stream_proxies_.clear();
  delegate_->OnRemoteStopped();
}

void QuicTransportProxy::OnConnectionFailed(const std::string& error_details,
                                            bool from_remote) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnConnectionFailed(error_details, from_remote);
  stream_proxies_.clear();
}

void QuicTransportProxy::OnStream(
    std::unique_ptr<QuicStreamProxy> stream_proxy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  stream_proxy->Initialize(this);

  QuicStreamProxy* stream_proxy_ptr = stream_proxy.get();
  stream_proxies_.insert(
      std::make_pair(stream_proxy_ptr, std::move(stream_proxy)));
  delegate_->OnStream(stream_proxy_ptr);
}

}  // namespace blink
