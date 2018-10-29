// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_proxy.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

QuicStreamProxy::QuicStreamProxy() {
  DETACH_FROM_THREAD(thread_checker_);
}

QuicStreamProxy::~QuicStreamProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void QuicStreamProxy::set_host(base::WeakPtr<QuicStreamHost> stream_host) {
  DETACH_FROM_THREAD(thread_checker_);
  stream_host_ = stream_host;
}

void QuicStreamProxy::Initialize(QuicTransportProxy* transport_proxy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(transport_proxy);
  transport_proxy_ = transport_proxy;
}

void QuicStreamProxy::set_delegate(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  delegate_ = delegate;
}

scoped_refptr<base::SingleThreadTaskRunner> QuicStreamProxy::host_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(transport_proxy_);
  return transport_proxy_->host_thread();
}

void QuicStreamProxy::Reset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBind(&QuicStreamHost::Reset, stream_host_));
  Delete();
}

void QuicStreamProxy::Finish() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*host_thread(), FROM_HERE,
                      CrossThreadBind(&QuicStreamHost::Finish, stream_host_));
  writeable_ = false;
  if (!readable_ && !writeable_) {
    Delete();
  }
}

void QuicStreamProxy::OnRemoteReset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  // Need to copy the |delegate_| member since Delete() will destroy |this|.
  Delegate* delegate_copy = delegate_;
  Delete();
  delegate_copy->OnRemoteReset();
}

void QuicStreamProxy::OnRemoteFinish() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  // Need to copy the |delegate_| member since Delete() will destroy |this|.
  Delegate* delegate_copy = delegate_;
  readable_ = false;
  if (!readable_ && !writeable_) {
    Delete();
  }
  delegate_copy->OnRemoteFinish();
}

void QuicStreamProxy::Delete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // OnRemoveStream will delete |this|.
  transport_proxy_->OnRemoveStream(this);
}

}  // namespace blink
