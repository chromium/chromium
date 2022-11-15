// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIALS_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIALS_CONTAINER_H_

#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_requester.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Credential;
class CredentialCreationOptions;
class CredentialRequestOptions;
class ExceptionState;
class Navigator;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT CredentialsContainer final : public ScriptWrappable,
                                                  public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static CredentialsContainer* credentials(Navigator&);
  explicit CredentialsContainer(Navigator&);

  // CredentialsContainer.idl
  ScriptPromise get(ScriptState*,
                    const CredentialRequestOptions*,
                    ExceptionState&);
  ScriptPromise store(ScriptState*, Credential* = nullptr);
  ScriptPromise create(ScriptState*,
                       const CredentialCreationOptions*,
                       ExceptionState&);
  ScriptPromise preventSilentAccess(ScriptState*);

  void Trace(Visitor*) const override;

 private:
  class OtpRequestAbortAlgorithm;
  class PublicKeyRequestAbortAlgorithm;

  Member<WebIdentityRequester> web_identity_requester_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIALS_CONTAINER_H_
