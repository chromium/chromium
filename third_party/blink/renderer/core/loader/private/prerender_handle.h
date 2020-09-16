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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRIVATE_PRERENDER_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRIVATE_PRERENDER_HANDLE_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ExecutionContext;
class Document;
class PrerenderClient;

class PrerenderHandle final : public GarbageCollected<PrerenderHandle>,
                              public ExecutionContextLifecycleObserver,
                              public mojom::blink::PrerenderHandleClient {
  USING_PRE_FINALIZER(PrerenderHandle, Dispose);

 public:
  static PrerenderHandle* Create(
      Document&,
      PrerenderClient*,
      const KURL&,
      mojom::blink::PrerenderRelType prerender_rel_type);

  using PassKey = util::PassKey<PrerenderHandle>;
  PrerenderHandle(PassKey,
                  ExecutionContext*,
                  PrerenderClient*,
                  const KURL&,
                  HeapMojoRemote<mojom::blink::PrerenderHandle>,
                  mojo::PendingReceiver<mojom::blink::PrerenderHandleClient>);
  ~PrerenderHandle() override;
  void Dispose();

  void Cancel();
  const KURL& Url() const;

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  // mojom::blink::PrerenderHandleClient:
  void OnPrerenderStart() override;
  void OnPrerenderStopLoading() override;
  void OnPrerenderDomContentLoaded() override;
  void OnPrerenderStop() override;

  void Trace(Visitor*) const override;

 private:
  void Detach();

  KURL url_;
  WeakMember<PrerenderClient> client_;
  HeapMojoRemote<mojom::blink::PrerenderHandle> remote_handle_;
  HeapMojoReceiver<mojom::blink::PrerenderHandleClient, PrerenderHandle>
      receiver_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderHandle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRIVATE_PRERENDER_HANDLE_H_
