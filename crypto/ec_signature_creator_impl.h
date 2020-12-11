// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_
#define CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "crypto/ec_signature_creator.h"

namespace crypto {

class ECSignatureCreatorImpl : public ECSignatureCreator {
 public:
  explicit ECSignatureCreatorImpl(ECPrivateKey* key);
  ~ECSignatureCreatorImpl() override;

  bool Sign(const uint8_t* data,
            int data_len,
            std::vector<uint8_t>* signature) override;

  bool DecodeSignature(const std::vector<uint8_t>& der_sig,
                       std::vector<uint8_t>* out_raw_sig) override;

 private:
  ECPrivateKey* key_;

  DISALLOW_COPY_AND_ASSIGN(ECSignatureCreatorImpl);
};

}  // namespace crypto

#endif  // CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_
