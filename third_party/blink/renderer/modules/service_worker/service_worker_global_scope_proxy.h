/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ServiceWorkerGlobalScope;
class WebEmbeddedWorkerImpl;
class WebServiceWorkerContextClient;

// This class is created and destructed on the main thread, but live most
// of its time as a resident of the worker thread. All methods other than its
// ctor/dtor and Detach() are called on the worker thread.
//
// This implements WebServiceWorkerContextProxy, which connects ServiceWorker's
// WorkerGlobalScope and embedder/chrome, and implements ServiceWorker-specific
// events/upcall methods that are to be called by embedder/chromium,
// e.g. onfetch.
//
// An instance of this class is supposed to outlive until
// workerThreadTerminated() is called by its corresponding
// WorkerGlobalScope.
//
// TODO(bashi): Update the above comment and method comments once we move
// creation of this class off the main thread.
class ServiceWorkerGlobalScopeProxy final : public WebServiceWorkerContextProxy,
                                            public WorkerReportingProxy {
 public:
  ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl&,
                                WebServiceWorkerContextClient&,
                                scoped_refptr<base::SingleThreadTaskRunner>
                                    parent_thread_default_task_runner);
  ~ServiceWorkerGlobalScopeProxy() override;

  // WebServiceWorkerContextProxy overrides:
  void BindServiceWorker(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void BindControllerServiceWorker(
      mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<WebURLResponse>,
      mojo::ScopedDataPipeConsumerHandle data_pipe) override;
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<WebServiceWorkerError>) override;
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   base::TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override;
  bool IsWindowInteractionAllowed() override;
  void PauseEvaluation() override;
  void ResumeEvaluation() override;

  // WorkerReportingProxy overrides:
  void CountFeature(WebFeature) override;
  void CountDeprecation(WebFeature) override;
  void ReportException(const String& error_message,
                       std::unique_ptr<SourceLocation>,
                       int exception_id) override;
  void ReportConsoleMessage(mojom::ConsoleMessageSource,
                            mojom::ConsoleMessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void WillInitializeWorkerContext() override;
  void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override;
  void DidLoadClassicScript() override;
  void DidFetchScript() override;
  void DidFailToFetchClassicScript() override;
  void DidFailToFetchModuleScript() override;
  void WillEvaluateClassicScript(size_t script_size,
                                 size_t cached_metadata_size) override;
  void WillEvaluateImportedClassicScript(size_t script_size,
                                         size_t cached_metadata_size) override;
  void WillEvaluateModuleScript() override;
  void DidEvaluateClassicScript(bool success) override;
  void DidEvaluateModuleScript(bool success) override;
  void DidCloseWorkerGlobalScope() override;
  void WillDestroyWorkerGlobalScope() override;
  void DidTerminateWorkerThread() override;
  bool IsServiceWorkerGlobalScopeProxy() const override;

  // Called from ServiceWorkerGlobalScope.
  void SetupNavigationPreload(
      int fetch_event_id,
      const KURL& url,
      mojom::blink::FetchEventPreloadHandlePtr preload_handle);
  void RequestTermination(WTF::CrossThreadOnceFunction<void(bool)> callback);

  // Detaches this proxy object entirely from the outside world, clearing out
  // all references.
  //
  // It is called on the main thread during WebEmbeddedWorkerImpl finalization
  // _after_ the worker thread using the proxy has been terminated.
  void Detach();

  void TerminateWorkerContext();

 private:
  WebServiceWorkerContextClient& Client() const;
  ServiceWorkerGlobalScope* WorkerGlobalScope() const;

  // Non-null until the WebEmbeddedWorkerImpl explicitly detach()es
  // as part of its finalization.
  WebEmbeddedWorkerImpl* embedded_worker_;

  scoped_refptr<base::SingleThreadTaskRunner>
      parent_thread_default_task_runner_;

  WebServiceWorkerContextClient* client_;

  CrossThreadPersistent<ServiceWorkerGlobalScope> worker_global_scope_;

  THREAD_CHECKER(worker_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerGlobalScopeProxy);
};

// TODO(leonhsl): This is only used by ServiceWorkerGlobalScope for calling
// WebServiceWorkerContextClient::{SetupNavigationPreload,RequestTermination}(),
// which will be Onion Soupped eventually, at that time we'd remove this.
template <>
struct DowncastTraits<ServiceWorkerGlobalScopeProxy> {
  static bool AllowFrom(const WorkerReportingProxy& proxy) {
    return proxy.IsServiceWorkerGlobalScopeProxy();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_PROXY_H_
