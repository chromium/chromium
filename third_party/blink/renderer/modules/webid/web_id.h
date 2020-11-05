// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class WebIDRequestOptions;
class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;

class WebID final : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit WebID(ExecutionContext&);

  // WebID IDL interface.
  ScriptPromise get(ScriptState*, const WebIDRequestOptions*, ExceptionState&);

  void Trace(blink::Visitor*) const override;

 private:
  void OnConnectionError();

  HeapMojoRemote<mojom::blink::FederatedAuthRequest> auth_request_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_H_
