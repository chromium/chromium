// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_
#define CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "crypto/ec_signature_creator.h"

namespace crypto {

class ECSignatureCreatorImpl : public ECSignatureCreator {
 public:
  explicit ECSignatureCreatorImpl(ECPrivateKey* key);

  ECSignatureCreatorImpl(const ECSignatureCreatorImpl&) = delete;
  ECSignatureCreatorImpl& operator=(const ECSignatureCreatorImpl&) = delete;

  ~ECSignatureCreatorImpl() override;

  bool Sign(base::span<const uint8_t> data,
            std::vector<uint8_t>* signature) override;

  bool DecodeSignature(const std::vector<uint8_t>& der_sig,
                       std::vector<uint8_t>* out_raw_sig) override;

 private:
  raw_ptr<ECPrivateKey> key_;
};

}  // namespace crypto

#endif  // CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_
