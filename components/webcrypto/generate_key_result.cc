// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/generate_key_result.h"

#include "base/check_op.h"
#include "base/notreached.h"

namespace webcrypto {

GenerateKeyResult::GenerateKeyResult() : type_(TYPE_NULL) {
}

GenerateKeyResult::Type GenerateKeyResult::type() const {
  return type_;
}

const blink::WebCryptoKey& GenerateKeyResult::secret_key() const {
  DCHECK_EQ(TYPE_SECRET_KEY, type_);
  return secret_key_;
}

const blink::WebCryptoKey& GenerateKeyResult::public_key() const {
  DCHECK_EQ(TYPE_PUBLIC_PRIVATE_KEY_PAIR, type_);
  return public_key_;
}

const blink::WebCryptoKey& GenerateKeyResult::private_key() const {
  DCHECK_EQ(TYPE_PUBLIC_PRIVATE_KEY_PAIR, type_);
  return private_key_;
}

void GenerateKeyResult::AssignSecretKey(const blink::WebCryptoKey& key) {
  type_ = TYPE_SECRET_KEY;
  secret_key_ = key;
}

void GenerateKeyResult::AssignKeyPair(const blink::WebCryptoKey& public_key,
                                      const blink::WebCryptoKey& private_key) {
  type_ = TYPE_PUBLIC_PRIVATE_KEY_PAIR;
  public_key_ = public_key;
  private_key_ = private_key;
}

void GenerateKeyResult::Complete(blink::WebCryptoResult* out) const {
  switch (type_) {
    case TYPE_NULL:
      NOTREACHED();
      break;
    case TYPE_SECRET_KEY:
      out->CompleteWithKey(secret_key());
      break;
    case TYPE_PUBLIC_PRIVATE_KEY_PAIR:
      out->CompleteWithKeyPair(public_key(), private_key());
      break;
  }
}

}  // namespace webcrypto
