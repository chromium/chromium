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

#include "third_party/blink/public/platform/web_crypto_key.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

class WebCryptoKeyPrivate : public ThreadSafeRefCounted<WebCryptoKeyPrivate> {
 public:
  WebCryptoKeyPrivate(std::unique_ptr<WebCryptoKeyHandle> handle,
                      WebCryptoKeyType type,
                      bool extractable,
                      const WebCryptoKeyAlgorithm& algorithm,
                      WebCryptoKeyUsageMask usages)
      : handle(std::move(handle)),
        type(type),
        extractable(extractable),
        algorithm(algorithm),
        usages(usages) {
    DCHECK(!algorithm.IsNull());
  }

  const std::unique_ptr<WebCryptoKeyHandle> handle;
  const WebCryptoKeyType type;
  const bool extractable;
  const WebCryptoKeyAlgorithm algorithm;
  const WebCryptoKeyUsageMask usages;
};

WebCryptoKey WebCryptoKey::Create(WebCryptoKeyHandle* handle,
                                  WebCryptoKeyType type,
                                  bool extractable,
                                  const WebCryptoKeyAlgorithm& algorithm,
                                  WebCryptoKeyUsageMask usages) {
  WebCryptoKey key;
  key.private_ = base::AdoptRef(new WebCryptoKeyPrivate(
      base::WrapUnique(handle), type, extractable, algorithm, usages));
  return key;
}

WebCryptoKey WebCryptoKey::CreateNull() {
  return WebCryptoKey();
}

WebCryptoKeyHandle* WebCryptoKey::Handle() const {
  DCHECK(!IsNull());
  return private_->handle.get();
}

WebCryptoKeyType WebCryptoKey::GetType() const {
  DCHECK(!IsNull());
  return private_->type;
}

bool WebCryptoKey::Extractable() const {
  DCHECK(!IsNull());
  return private_->extractable;
}

const WebCryptoKeyAlgorithm& WebCryptoKey::Algorithm() const {
  DCHECK(!IsNull());
  return private_->algorithm;
}

WebCryptoKeyUsageMask WebCryptoKey::Usages() const {
  DCHECK(!IsNull());
  return private_->usages;
}

bool WebCryptoKey::IsNull() const {
  return private_.IsNull();
}

bool WebCryptoKey::KeyUsageAllows(const blink::WebCryptoKeyUsage usage) const {
  return ((private_->usages & usage) != 0);
}

void WebCryptoKey::Assign(const WebCryptoKey& other) {
  private_ = other.private_;
}

void WebCryptoKey::Reset() {
  private_.Reset();
}

}  // namespace blink
