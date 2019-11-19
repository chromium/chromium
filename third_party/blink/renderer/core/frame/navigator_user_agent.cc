// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_user_agent.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/user_agent.h"

namespace blink {

ScriptPromise NavigatorUserAgent::getUserAgent(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  blink::UserAgentMetadata metadata = GetUserAgentMetadata();
  blink::UserAgent* idl_metadata = blink::UserAgent::Create();

  idl_metadata->setBrand(String::FromUTF8(metadata.brand));
  idl_metadata->setVersion(String::FromUTF8(metadata.full_version));
  idl_metadata->setPlatform(String::FromUTF8(metadata.platform));
  idl_metadata->setArchitecture(String::FromUTF8(metadata.architecture));
  idl_metadata->setModel(String::FromUTF8(metadata.model));
  resolver->Resolve(idl_metadata);

  return promise;
}

}  // namespace blink
