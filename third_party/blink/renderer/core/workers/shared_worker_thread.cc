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

#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"

#include <memory>
#include <utility>
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"

namespace blink {

SharedWorkerThread::SharedWorkerThread(
    WorkerReportingProxy& worker_reporting_proxy,
    const SharedWorkerToken& token)
    : WorkerThread(worker_reporting_proxy),
      worker_backing_thread_(std::make_unique<WorkerBackingThread>(
          ThreadCreationParams(GetThreadType()))),
      token_(token) {}

SharedWorkerThread::~SharedWorkerThread() = default;

WorkerOrWorkletGlobalScope* SharedWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  // We need to pull this bool out of creation_params before we construct
  // SharedWorkerGlobalScope as it has to move the pointer to the base class
  // before any information in it can be accessed.
  bool require_cross_site_request_for_cookies =
      creation_params->require_cross_site_request_for_cookies;
  return MakeGarbageCollected<SharedWorkerGlobalScope>(
      std::move(creation_params), this, time_origin_, token_,
      require_cross_site_request_for_cookies);
}

}  // namespace blink
