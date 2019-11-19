// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIALS_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIALS_CONTAINER_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Credential;
class CredentialCreationOptions;
class CredentialRequestOptions;
class ExceptionState;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT CredentialsContainer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CredentialsContainer();

  // CredentialsContainer.idl
  ScriptPromise get(ScriptState*, const CredentialRequestOptions*);
  ScriptPromise store(ScriptState*, Credential* = nullptr);
  ScriptPromise create(ScriptState*,
                       const CredentialCreationOptions*,
                       ExceptionState&);
  ScriptPromise preventSilentAccess(ScriptState*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIALS_CONTAINER_H_
