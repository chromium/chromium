// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_host.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

QuicStreamHost::QuicStreamHost() {
  DETACH_FROM_THREAD(thread_checker_);
}

QuicStreamHost::~QuicStreamHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void QuicStreamHost::set_proxy(base::WeakPtr<QuicStreamProxy> stream_proxy) {
  DETACH_FROM_THREAD(thread_checker_);
  stream_proxy_ = stream_proxy;
}

void QuicStreamHost::Initialize(QuicTransportHost* transport_host,
                                P2PQuicStream* p2p_stream) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(transport_host);
  DCHECK(p2p_stream);
  transport_host_ = transport_host;
  p2p_stream_ = p2p_stream;
  p2p_stream_->SetDelegate(this);
}

scoped_refptr<base::SingleThreadTaskRunner> QuicStreamHost::proxy_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(transport_host_);
  return transport_host_->proxy_thread();
}

void QuicStreamHost::Reset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(p2p_stream_);
  p2p_stream_->Reset();
  Delete();
}

void QuicStreamHost::MarkReceivedDataConsumed(uint32_t amount) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(p2p_stream_);
  p2p_stream_->MarkReceivedDataConsumed(amount);
}

void QuicStreamHost::WriteData(Vector<uint8_t> data, bool fin) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(p2p_stream_);
  p2p_stream_->WriteData(std::move(data), fin);
  if (fin) {
    DCHECK(writable_);
    writable_ = false;
    if (!readable_ && !writable_) {
      Delete();
    }
  }
}

void QuicStreamHost::OnRemoteReset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread(), FROM_HERE,
      CrossThreadBindOnce(&QuicStreamProxy::OnRemoteReset, stream_proxy_));
  Delete();
}

void QuicStreamHost::OnDataReceived(Vector<uint8_t> data, bool fin) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*proxy_thread(), FROM_HERE,
                      CrossThreadBindOnce(&QuicStreamProxy::OnDataReceived,
                                          stream_proxy_, std::move(data), fin));
  if (fin) {
    readable_ = false;
    if (!readable_ && !writable_) {
      Delete();
    }
  }
}

void QuicStreamHost::OnWriteDataConsumed(uint32_t amount) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*proxy_thread(), FROM_HERE,
                      CrossThreadBindOnce(&QuicStreamProxy::OnWriteDataConsumed,
                                          stream_proxy_, amount));
}

void QuicStreamHost::Delete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(transport_host_);
  p2p_stream_->SetDelegate(nullptr);
  // OnRemoveStream will delete |this|.
  transport_host_->OnRemoveStream(this);
}

}  // namespace blink
