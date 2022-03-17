// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_FEDERATED_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_FEDERATED_CREDENTIAL_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_logout_rps_request.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class CredentialRequestOptions;
class FederatedAccountLoginRequest;
class FederatedCredentialInit;

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

  static FederatedCredential* Create(const KURL& provider_url,
                                     const String& client_id,
                                     const String& hint,
                                     const CredentialRequestOptions* options);

  FederatedCredential(const String& id,
                      scoped_refptr<const SecurityOrigin> provider,
                      const String& name,
                      const KURL& icon_url);

  FederatedCredential(const KURL& provider_url,
                      const String& client_id,
                      const String& hint,
                      const CredentialRequestOptions* options);

  void Trace(Visitor*) const override;

  scoped_refptr<const SecurityOrigin> GetProviderAsOrigin() const {
    return provider_origin_;
  }

  // Credential:
  bool IsFederatedCredential() const override;

  // FederatedCredential.idl
  String provider() const {
    CHECK(provider_origin_);
    return provider_origin_->ToString();
  }
  const String& name() const { return name_; }
  const KURL& iconURL() const { return icon_url_; }
  const String& protocol() const {
    // TODO(mkwst): This is a stub, as we don't yet have any support on the
    // Chromium-side.
    return g_empty_string;
  }

  ScriptPromise login(ScriptState* script_state,
                      FederatedAccountLoginRequest* request);

  ScriptPromise logout(ScriptState* script_state);

  ScriptPromise revoke(ScriptState*, const String& hint, ExceptionState&);

  static ScriptPromise logoutRps(
      ScriptState*,
      const HeapVector<Member<FederatedCredentialLogoutRpsRequest>>&);

 private:
  const scoped_refptr<const SecurityOrigin> provider_origin_;
  const String name_;
  const KURL icon_url_;
  const KURL provider_url_;
  const String client_id_;
  Member<const CredentialRequestOptions> options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_FEDERATED_CREDENTIAL_H_
