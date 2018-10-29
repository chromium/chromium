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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EMBEDDED_WORKER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EMBEDDED_WORKER_H_

#include <memory>

#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

class WebContentSettingsClient;
class WebServiceWorkerContextClient;
class WebURL;
struct WebConsoleMessage;
struct WebEmbeddedWorkerStartData;

// As we're on the border line between non-Blink and Blink variants, we need
// to use mojo::ScopedMessagePipeHandle to pass Mojo types.
struct WebServiceWorkerInstalledScriptsManagerParams {
  WebVector<WebURL> installed_scripts_urls;
  // A handle for mojom::blink::ServiceWorkerInstalledScriptsManagerRequest.
  mojo::ScopedMessagePipeHandle manager_request;
  // A handle for mojom::blink::ServiceWorkerInstalledScriptsManagerHostPtrInfo.
  mojo::ScopedMessagePipeHandle manager_host_ptr;
};

// An interface to start and terminate an embedded worker.
// All methods of this class must be called on the main thread.
class BLINK_EXPORT WebEmbeddedWorker {
 public:
  // Invoked on the main thread to instantiate a WebEmbeddedWorker.
  // The given WebWorkerContextClient and WebContentSettingsClient are going to
  // be passed on to the worker thread and is held by a newly created
  // WorkerGlobalScope.
  static std::unique_ptr<WebEmbeddedWorker> Create(
      std::unique_ptr<WebServiceWorkerContextClient>,
      std::unique_ptr<WebServiceWorkerInstalledScriptsManagerParams>,
      mojo::ScopedMessagePipeHandle content_settings_handle,
      mojo::ScopedMessagePipeHandle cache_storage,
      mojo::ScopedMessagePipeHandle interface_provider);

  virtual ~WebEmbeddedWorker() = default;

  // Starts and terminates WorkerThread and WorkerGlobalScope.
  virtual void StartWorkerContext(const WebEmbeddedWorkerStartData&) = 0;
  virtual void TerminateWorkerContext() = 0;

  // Resumes starting a worker startup that was paused via
  // WebEmbeddedWorkerStartData.pauseAfterDownloadMode.
  virtual void ResumeAfterDownload() = 0;

  // Inspector related methods.
  virtual void AddMessageToConsole(const WebConsoleMessage&) = 0;
  virtual void BindDevToolsAgent(
      mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
      mojo::ScopedInterfaceEndpointHandle devtools_agent_request) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EMBEDDED_WORKER_H_
