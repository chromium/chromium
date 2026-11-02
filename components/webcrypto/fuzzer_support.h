// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_FUZZER_SUPPORT_H_
#define COMPONENTS_WEBCRYPTO_FUZZER_SUPPORT_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_crypto_key.h"

namespace webcrypto {

void ImportEcKeyFromDerFuzzData(base::span<const uint8_t> data,
                                blink::WebCryptoKeyFormat format);

void ImportEcKeyFromRawFuzzData(base::span<const uint8_t> data);

void ImportRsaKeyFromDerFuzzData(base::span<const uint8_t> data,
                                 blink::WebCryptoKeyFormat format);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_FUZZER_SUPPORT_H_
