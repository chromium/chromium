// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/crypto_data.h"

namespace webcrypto {

CryptoData::CryptoData() : bytes_(nullptr), byte_length_(0) {}

CryptoData::CryptoData(const unsigned char* bytes, unsigned int byte_length)
    : bytes_(bytes), byte_length_(byte_length) {
}

CryptoData::CryptoData(const std::vector<unsigned char>& bytes)
    : bytes_(bytes.data()),
      byte_length_(static_cast<unsigned int>(bytes.size())) {}

CryptoData::CryptoData(const std::string& bytes)
    : bytes_(bytes.size() ? reinterpret_cast<const unsigned char*>(bytes.data())
                          : nullptr),
      byte_length_(static_cast<unsigned int>(bytes.size())) {}

CryptoData::CryptoData(const blink::WebVector<unsigned char>& bytes)
    : bytes_(bytes.data()),
      byte_length_(static_cast<unsigned int>(bytes.size())) {}

}  // namespace webcrypto
