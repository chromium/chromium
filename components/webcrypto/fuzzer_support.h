// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_FUZZER_SUPPORT_H_
#define COMPONENTS_WEBCRYPTO_FUZZER_SUPPORT_H_

#include <stddef.h>
#include <stdint.h>

#include "third_party/blink/public/platform/web_crypto_key.h"

namespace webcrypto {

void ImportEcKeyFromDerFuzzData(const uint8_t* data,
                                size_t size,
                                blink::WebCryptoKeyFormat format);

void ImportEcKeyFromRawFuzzData(const uint8_t* data, size_t size);

void ImportRsaKeyFromDerFuzzData(const uint8_t* data,
                                 size_t size,
                                 blink::WebCryptoKeyFormat format);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_FUZZER_SUPPORT_H_
