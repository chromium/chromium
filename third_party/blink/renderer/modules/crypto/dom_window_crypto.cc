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

#include "third_party/blink/renderer/modules/crypto/dom_window_crypto.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/crypto/crypto.h"

namespace blink {

DOMWindowCrypto::DOMWindowCrypto(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

const char DOMWindowCrypto::kSupplementName[] = "DOMWindowCrypto";

DOMWindowCrypto& DOMWindowCrypto::From(LocalDOMWindow& window) {
  DOMWindowCrypto* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowCrypto>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowCrypto>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

Crypto* DOMWindowCrypto::crypto(LocalDOMWindow& window) {
  return DOMWindowCrypto::From(window).crypto();
}

Crypto* DOMWindowCrypto::crypto() const {
  if (!crypto_)
    crypto_ = MakeGarbageCollected<Crypto>();
  return crypto_.Get();
}

void DOMWindowCrypto::Trace(blink::Visitor* visitor) {
  visitor->Trace(crypto_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
