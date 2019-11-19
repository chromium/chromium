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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_GLOBAL_SCOPE_H_

#include <memory>
#include "third_party/blink/renderer/core/animation_frame/worker_animation_frame_provider.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class DedicatedWorkerObjectProxy;
class DedicatedWorkerThread;
class PostMessageOptions;
class ScriptState;
class WorkerClassicScriptLoader;
struct GlobalScopeCreationParams;

class CORE_EXPORT DedicatedWorkerGlobalScope final : public WorkerGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // TODO(nhiroki): Merge Create() into the constructor after
  // off-the-main-thread worker script fetch is enabled by default.
  static DedicatedWorkerGlobalScope* Create(
      std::unique_ptr<GlobalScopeCreationParams>,
      DedicatedWorkerThread*,
      base::TimeTicks time_origin);

  // Do not call this. Use Create() instead. This is public only for
  // MakeGarbageCollected.
  DedicatedWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>,
      DedicatedWorkerThread*,
      base::TimeTicks time_origin,
      std::unique_ptr<Vector<String>> outside_origin_trial_tokens,
      const BeginFrameProviderParams& begin_frame_provider_params);

  ~DedicatedWorkerGlobalScope() override;

  // Implements ExecutionContext.
  bool IsDedicatedWorkerGlobalScope() const override { return true; }

  // Implements EventTarget
  // (via WorkerOrWorkletGlobalScope -> EventTargetWithInlineData).
  const AtomicString& InterfaceName() const override;

  // RequestAnimationFrame
  int requestAnimationFrame(V8FrameRequestCallback* callback, ExceptionState&);
  void cancelAnimationFrame(int id);
  WorkerAnimationFrameProvider* GetAnimationFrameProvider() {
    return animation_frame_provider_;
  }

  // Implements WorkerGlobalScope.
  void Initialize(const KURL& response_url,
                  network::mojom::ReferrerPolicy response_referrer_policy,
                  network::mojom::IPAddressSpace response_address_space,
                  const Vector<CSPHeaderAndType>& response_csp_headers,
                  const Vector<String>* response_origin_trial_tokens,
                  int64_t appcache_host) override;
  void FetchAndRunClassicScript(
      const KURL& script_url,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      const v8_inspector::V8StackTraceId& stack_id) override;
  void FetchAndRunModuleScript(
      const KURL& module_url_record,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      network::mojom::CredentialsMode) override;

  // Called by the bindings (dedicated_worker_global_scope.idl).
  const String name() const;
  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   HeapVector<ScriptValue>& transfer,
                   ExceptionState&);
  void postMessage(ScriptState*,
                   const ScriptValue& message,
                   const PostMessageOptions*,
                   ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message, kMessage)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(messageerror, kMessageerror)

  // Called by the Oilpan.
  void Trace(blink::Visitor*) override;

 private:
  void DidReceiveResponseForClassicScript(
      WorkerClassicScriptLoader* classic_script_loader);
  void DidFetchClassicScript(WorkerClassicScriptLoader* classic_script_loader,
                             const v8_inspector::V8StackTraceId& stack_id);

  DedicatedWorkerObjectProxy& WorkerObjectProxy() const;

  Member<WorkerAnimationFrameProvider> animation_frame_provider_;
};

template <>
struct DowncastTraits<DedicatedWorkerGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsDedicatedWorkerGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_DEDICATED_WORKER_GLOBAL_SCOPE_H_
