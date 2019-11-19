/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_

#include "third_party/blink/public/web/web_shared_worker.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/workers/shared_worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace blink {

class SharedWorkerThread;
class WebSharedWorkerClient;
class WebString;
class WebURL;

// This class is used by the worker process code to talk to the SharedWorker
// implementation. This is basically accessed on the main thread, but some
// methods must be called from a worker thread. Such methods are suffixed with
// *OnWorkerThread or have header comments.
//
// Owned by WebSharedWorkerClient. Destroyed in TerminateWorkerThread() or
// DidTerminateWorkerThread() via
// WebSharedWorkerClient::WorkerContextDestroyed().
class CORE_EXPORT WebSharedWorkerImpl final : public WebSharedWorker {
 public:
  explicit WebSharedWorkerImpl(WebSharedWorkerClient*);
  ~WebSharedWorkerImpl() override;

  // WebSharedWorker methods:
  void StartWorkerContext(
      const WebURL&,
      const WebString& name,
      const WebString& user_agent,
      const WebString& content_security_policy,
      network::mojom::ContentSecurityPolicyType,
      network::mojom::IPAddressSpace,
      const base::UnguessableToken& appcache_host_id,
      const base::UnguessableToken& devtools_worker_token,
      mojo::ScopedMessagePipeHandle content_settings_handle,
      mojo::ScopedMessagePipeHandle interface_provider,
      mojo::ScopedMessagePipeHandle browser_interface_broker,
      bool pause_worker_context_on_start) override;
  void Connect(MessagePortChannel) override;
  void TerminateWorkerContext() override;

  // Callback methods for SharedWorkerReportingProxy.
  void CountFeature(WebFeature);
  void DidFailToFetchClassicScript();
  void DidEvaluateClassicScript(bool success);
  void DidCloseWorkerGlobalScope();
  // This synchronously destroys |this|.
  void DidTerminateWorkerThread();

 private:
  SharedWorkerThread* GetWorkerThread() { return worker_thread_.get(); }

  // Shuts down the worker thread. This may synchronously destroy |this|.
  void TerminateWorkerThread();

  void ConnectTaskOnWorkerThread(MessagePortChannel);

  Persistent<SharedWorkerReportingProxy> reporting_proxy_;
  std::unique_ptr<SharedWorkerThread> worker_thread_;

  // |client_| owns |this|.
  WebSharedWorkerClient* client_;

  bool asked_to_terminate_ = false;

  base::WeakPtrFactory<WebSharedWorkerImpl> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_
