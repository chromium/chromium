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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_GLOBAL_SCOPE_H_

#include <memory>
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class SharedWorkerThread;

class SharedWorkerGlobalScope final : public WorkerGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SharedWorkerGlobalScope(const String& name,
                          std::unique_ptr<GlobalScopeCreationParams>,
                          SharedWorkerThread*,
                          base::TimeTicks time_origin);
  ~SharedWorkerGlobalScope() override;

  bool IsSharedWorkerGlobalScope() const override { return true; }

  // EventTarget
  const AtomicString& InterfaceName() const override;

  // WorkerGlobalScope
  void ImportModuleScript(
      const KURL& module_url_record,
      FetchClientSettingsObjectSnapshot* outside_settings_object,
      network::mojom::FetchCredentialsMode) override;

  // Setters/Getters for attributes in SharedWorkerGlobalScope.idl
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
  String name() const { return name_; }

  void ConnectPausable(MessagePortChannel channel);

  void Trace(blink::Visitor*) override;

 private:
  void ExceptionThrown(ErrorEvent*) override;

  const String name_;
};

template <>
struct DowncastTraits<SharedWorkerGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsSharedWorkerGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_GLOBAL_SCOPE_H_
