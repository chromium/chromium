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

#include "third_party/blink/renderer/core/workers/dedicated_worker_thread.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"

namespace blink {

DedicatedWorkerThread::DedicatedWorkerThread(
    ExecutionContext* parent_execution_context,
    DedicatedWorkerObjectProxy& worker_object_proxy,
    mojo::PendingRemote<mojom::blink::DedicatedWorkerHost>
        dedicated_worker_host,
    mojo::PendingRemote<mojom::blink::BackForwardCacheControllerHost>
        back_forward_cache_controller_host)
    : WorkerThread(worker_object_proxy),
      worker_object_proxy_(worker_object_proxy),
      pending_dedicated_worker_host_(std::move(dedicated_worker_host)),
      pending_back_forward_cache_controller_host_(
          std::move(back_forward_cache_controller_host)) {
  FrameOrWorkerScheduler* scheduler =
      parent_execution_context ? parent_execution_context->GetScheduler()
                               : nullptr;
  worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
      ThreadCreationParams(GetThreadType())
          .SetFrameOrWorkerScheduler(scheduler));
}

DedicatedWorkerThread::~DedicatedWorkerThread() = default;

WorkerOrWorkletGlobalScope* DedicatedWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  DCHECK(pending_dedicated_worker_host_);
  return DedicatedWorkerGlobalScope::Create(
      std::move(creation_params), this, time_origin_,
      std::move(pending_dedicated_worker_host_),
      std::move(pending_back_forward_cache_controller_host_));
}

}  // namespace blink
