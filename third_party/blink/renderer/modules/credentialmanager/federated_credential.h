// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_FEDERATED_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_FEDERATED_CREDENTIAL_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_id_logout_request.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class FederatedCredentialInit;
class FederatedIdentityProvider;

class MODULES_EXPORT FederatedCredential final : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FederatedCredential* Create(const FederatedCredentialInit*,
                                     ExceptionState&);
  static FederatedCredential* Create(
      const String& id,
      scoped_refptr<const SecurityOrigin> provider,
      const String& name,
      const KURL& icon_url);

  FederatedCredential(const String& id,
                      scoped_refptr<const SecurityOrigin> provider,
                      const String& name,
                      const KURL& icon_url);

  scoped_refptr<const SecurityOrigin> GetProviderAsOrigin() const {
    return provider_;
  }

  // Credential:
  bool IsFederatedCredential() const override;

  // FederatedCredential.idl
  String provider() const {
    CHECK(provider_);
    return provider_->ToString();
  }
  const String& name() const { return name_; }
  const KURL& iconURL() const { return icon_url_; }
  const String& protocol() const {
    // TODO(mkwst): This is a stub, as we don't yet have any support on the
    // Chromium-side.
    return g_empty_string;
  }

  const String& idToken() const {
    // TODO(goto): This is a stub, so that we can port the WebID API
    // gradually.
    return g_empty_string;
  }

  const String& approvedBy() const {
    // TODO(goto): This is a stub, so that we can port the WebID API
    // gradually.
    return g_empty_string;
  }

  static ScriptPromise logout(ScriptState*,
                              const HeapVector<Member<WebIdLogoutRequest>>&);
  static ScriptPromise revoke(ScriptState*,
                              const String&,
                              FederatedIdentityProvider*,
                              ExceptionState&);

 private:
  const scoped_refptr<const SecurityOrigin> provider_;
  const String name_;
  const KURL icon_url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_FEDERATED_CREDENTIAL_H_
