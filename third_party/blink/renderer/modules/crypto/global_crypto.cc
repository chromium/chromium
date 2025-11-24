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

#include "third_party/blink/renderer/modules/crypto/global_crypto.h"

#include "third_party/blink/renderer/core/frame/window_or_worker_global_scope.h"
#include "third_party/blink/renderer/modules/crypto/crypto.h"

namespace blink {

GlobalCrypto& GlobalCrypto::From(WindowOrWorkerGlobalScope& context) {
  GlobalCrypto* supplement = context.GetGlobalCrypto();
  if (!supplement) {
    supplement = MakeGarbageCollected<GlobalCrypto>();
    context.SetGlobalCrypto(supplement);
  }
  return *supplement;
}

Crypto* GlobalCrypto::crypto(WindowOrWorkerGlobalScope& context) {
  return GlobalCrypto::From(context).crypto();
}

Crypto* GlobalCrypto::crypto() const {
  if (!crypto_) {
    crypto_ = MakeGarbageCollected<Crypto>();
  }
  return crypto_.Get();
}

void GlobalCrypto::Trace(Visitor* visitor) const {
  visitor->Trace(crypto_);
}

}  // namespace blink
