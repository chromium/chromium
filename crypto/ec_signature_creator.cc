// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_signature_creator.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "crypto/ec_signature_creator_impl.h"

namespace crypto {

// static
std::unique_ptr<ECSignatureCreator> ECSignatureCreator::Create(
    ECPrivateKey* key) {
  return std::make_unique<ECSignatureCreatorImpl>(key);
}

}  // namespace crypto
