// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/sctp_transport_proxy.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

// static
std::unique_ptr<SctpTransportProxy> SctpTransportProxy::Create(
    LocalFrame& frame,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    rtc::scoped_refptr<webrtc::SctpTransportInterface> sctp_transport,
    Delegate* delegate) {
  DCHECK(proxy_thread->BelongsToCurrentThread());
  std::unique_ptr<SctpTransportProxy> proxy =
      base::WrapUnique(new SctpTransportProxy(frame, proxy_thread, host_thread,
                                              sctp_transport, delegate));
  PostCrossThreadTask(
      *host_thread, FROM_HERE,
      CrossThreadBindOnce(&SctpTransportProxy::StartOnHostThread,
                          CrossThreadUnretained(proxy.get())));
  return proxy;
}

SctpTransportProxy::SctpTransportProxy(
    LocalFrame& frame,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    rtc::scoped_refptr<webrtc::SctpTransportInterface> sctp_transport,
    Delegate* delegate)
    : proxy_thread_(std::move(proxy_thread)),
      host_thread_(std::move(host_thread)),
      sctp_transport_(std::move(sctp_transport)),
      delegate_(MakeUnwrappingCrossThreadHandle(delegate)) {}

void SctpTransportProxy::StartOnHostThread() {
  DCHECK(host_thread_->BelongsToCurrentThread());
  sctp_transport_->RegisterObserver(this);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&Delegate::OnStartCompleted,
                          MakeUnwrappingCrossThreadHandle(delegate_),
                          sctp_transport_->Information()));
}

void SctpTransportProxy::OnStateChange(webrtc::SctpTransportInformation info) {
  DCHECK(host_thread_->BelongsToCurrentThread());
  // Closed is the last state that can happen, so unregister when we see this.
  // Unregistering allows us to safely delete the proxy independent of the
  // state of the webrtc::SctpTransport.
  if (info.state() == webrtc::SctpTransportState::kClosed) {
    sctp_transport_->UnregisterObserver();
  }
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&Delegate::OnStateChange,
                          MakeUnwrappingCrossThreadHandle(delegate_), info));
  if (info.state() == webrtc::SctpTransportState::kClosed) {
    // Don't hold on to |delegate| any more.
    delegate_.Clear();
  }
}

}  // namespace blink
