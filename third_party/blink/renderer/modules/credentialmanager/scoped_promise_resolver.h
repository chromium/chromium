// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_SCOPED_PROMISE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_SCOPED_PROMISE_RESOLVER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Off-heap wrapper that holds a strong reference to a ScriptPromiseResolver.
class ScopedPromiseResolver {
  USING_FAST_MALLOC(ScopedPromiseResolver);

 public:
  explicit ScopedPromiseResolver(ScriptPromiseResolver* resolver);

  ~ScopedPromiseResolver();

  // Releases the owned |resolver_|. This is to be called by the Mojo response
  // callback responsible for resolving the corresponding ScriptPromise
  //
  // If this method is not called before |this| goes of scope, it is assumed
  // that a Mojo connection error has occurred, and the response callback was
  // never invoked. The Promise will be rejected with an appropriate exception.
  ScriptPromiseResolver* Release();

 private:
  void OnConnectionError();

  Persistent<ScriptPromiseResolver> resolver_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPromiseResolver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_SCOPED_PROMISE_RESOLVER_H_
