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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SHARED_WORKER_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SHARED_WORKER_CLIENT_H_

#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"

namespace blink {

// Provides an interface back to the in-page script object for a worker.
// All functions are expected to be called back on the thread that created
// the Worker object, unless noted.
//
// An instance of this class must outlive WebSharedWorker (i.e. must be kept
// alive until WorkerScriptLoadFailed() or WorkerContextDestroyed() is called).
class WebSharedWorkerClient {
 public:
  virtual void CountFeature(mojom::WebFeature) = 0;
  virtual void WorkerContextClosed() = 0;
  virtual void WorkerContextDestroyed() = 0;
  virtual void WorkerReadyForInspection(
      mojo::ScopedMessagePipeHandle devtools_agent_ptr_info,
      mojo::ScopedMessagePipeHandle devtools_agent_host_request) {}
  virtual void WorkerScriptLoadFailed() = 0;
  virtual void WorkerScriptEvaluated(bool success) = 0;

  // Called on the main thread during initialization. Creates a new
  // WebWorkerFetchContext for the shared worker. This is passed to the worker
  // thread and used loading requests from the shared worker.
  virtual scoped_refptr<WebWorkerFetchContext> CreateWorkerFetchContext() = 0;
};

}  // namespace blink

#endif
