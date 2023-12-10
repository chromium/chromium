// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/dtls_transport_proxy.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

// Static
std::unique_ptr<DtlsTransportProxy> DtlsTransportProxy::Create(
    LocalFrame& frame,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    webrtc::DtlsTransportInterface* dtls_transport,
    Delegate* delegate) {
  DCHECK(proxy_thread->BelongsToCurrentThread());
  std::unique_ptr<DtlsTransportProxy> proxy =
      base::WrapUnique(new DtlsTransportProxy(frame, proxy_thread, host_thread,
                                              dtls_transport, delegate));
  // TODO(hta, tommi): Delete this thread jump once creation can be initiated
  // from the host thread (=webrtc network thread).
  PostCrossThreadTask(
      *host_thread, FROM_HERE,
      CrossThreadBindOnce(&DtlsTransportProxy::StartOnHostThread,
                          CrossThreadUnretained(proxy.get())));
  return proxy;
}

DtlsTransportProxy::DtlsTransportProxy(
    LocalFrame& frame,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    webrtc::DtlsTransportInterface* dtls_transport,
    Delegate* delegate)
    : proxy_thread_(std::move(proxy_thread)),
      host_thread_(std::move(host_thread)),
      dtls_transport_(dtls_transport),
      delegate_(MakeCrossThreadHandle(delegate)) {}

void DtlsTransportProxy::StartOnHostThread() {
  DCHECK(host_thread_->BelongsToCurrentThread());
  dtls_transport_->RegisterObserver(this);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&Delegate::OnStartCompleted,
                          MakeUnwrappingCrossThreadHandle(delegate_),
                          dtls_transport_->Information()));
}

void DtlsTransportProxy::OnStateChange(webrtc::DtlsTransportInformation info) {
  DCHECK(host_thread_->BelongsToCurrentThread());
  // Closed is the last state that can happen, so unregister when we see this.
  // Unregistering allows us to safely delete the proxy independent of the
  // state of the webrtc::DtlsTransport.
  if (info.state() == webrtc::DtlsTransportState::kClosed) {
    dtls_transport_->UnregisterObserver();
  }
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&Delegate::OnStateChange,
                          MakeUnwrappingCrossThreadHandle(delegate_), info));
  if (info.state() == webrtc::DtlsTransportState::kClosed) {
    // This effectively nullifies `delegate_`. We can't just assign nullptr the
    // normal way, because CrossThreadHandle does not support assignment.
    CrossThreadHandle<Delegate> expiring_handle = std::move(delegate_);
  }
}

void DtlsTransportProxy::OnError(webrtc::RTCError error) {
  DCHECK(host_thread_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

}  // namespace blink
