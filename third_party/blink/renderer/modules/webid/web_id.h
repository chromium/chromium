// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_H_

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/federated_auth_response.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class WebIdLogoutRequest;
class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;

class WebId final : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit WebId(ExecutionContext&);

  // WebID IDL interface.
  ScriptPromise provide(ScriptState*, String id_token);
  ScriptPromise logout(ScriptState*,
                       const HeapVector<Member<WebIdLogoutRequest>>&,
                       ExceptionState&);

  void Trace(blink::Visitor*) const override;

 private:
  template <typename Interface>
  void BindRemote(HeapMojoRemote<Interface>& remote);
  void OnConnectionError();

  // TODO(yigu): This request is for logout only at the moment. When migrating
  // logout to use CredentialManagement API, we should differentiate it with
  // the existing fedcm_get_request_ to avoid potential resource contention in
  // the browser process.
  HeapMojoRemote<mojom::blink::FederatedAuthRequest> auth_request_;
  HeapMojoRemote<mojom::blink::FederatedAuthResponse> auth_response_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_H_
