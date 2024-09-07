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

#include "third_party/blink/public/platform/web_crypto.h"

#include "third_party/blink/renderer/platform/crypto_result.h"

namespace blink {

void WebCryptoResult::CompleteWithError(WebCryptoErrorType error_type,
                                        const WebString& error_details) {
  if (!Cancelled())
    impl_->CompleteWithError(error_type, error_details);
  Reset();
}

void WebCryptoResult::CompleteWithBuffer(base::span<const uint8_t> bytes) {
  if (!Cancelled()) {
    impl_->CompleteWithBuffer(bytes);
  }
  Reset();
}

void WebCryptoResult::CompleteWithJson(std::string_view utf8_data) {
  if (!Cancelled()) {
    impl_->CompleteWithJson(utf8_data);
  }
  Reset();
}

void WebCryptoResult::CompleteWithBoolean(bool b) {
  if (!Cancelled())
    impl_->CompleteWithBoolean(b);
  Reset();
}

void WebCryptoResult::CompleteWithKey(const WebCryptoKey& key) {
  DCHECK(!key.IsNull());
  if (!Cancelled())
    impl_->CompleteWithKey(key);
  Reset();
}

void WebCryptoResult::CompleteWithKeyPair(const WebCryptoKey& public_key,
                                          const WebCryptoKey& private_key) {
  DCHECK(!public_key.IsNull());
  DCHECK(!private_key.IsNull());
  if (!Cancelled())
    impl_->CompleteWithKeyPair(public_key, private_key);
  Reset();
}

bool WebCryptoResult::Cancelled() const {
  return cancel_->Cancelled();
}

ExecutionContext* WebCryptoResult::GetExecutionContext() const {
  return impl_->GetExecutionContext();
}

WebCryptoResult::WebCryptoResult(CryptoResult* impl,
                                 scoped_refptr<CryptoResultCancel> cancel)
    : impl_(impl), cancel_(std::move(cancel)) {
  DCHECK(impl_.Get());
  DCHECK(cancel_.Get());
}

void WebCryptoResult::Reset() {
  impl_.Reset();
  cancel_.Reset();
}

void WebCryptoResult::Assign(const WebCryptoResult& o) {
  impl_ = o.impl_;
  cancel_ = o.cancel_;
}

}  // namespace blink
