// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATIONS_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATIONS_H_

#include <memory>

#include "third_party/blink/public/platform/web_crypto.h"

// The definitions for these functions live in the algorithms/ directory.
namespace webcrypto {

class AlgorithmImplementation;

std::unique_ptr<AlgorithmImplementation> CreateShaImplementation();
std::unique_ptr<AlgorithmImplementation> CreateAesCbcImplementation();
std::unique_ptr<AlgorithmImplementation> CreateAesCtrImplementation();
std::unique_ptr<AlgorithmImplementation> CreateAesGcmImplementation();
std::unique_ptr<AlgorithmImplementation> CreateAesKwImplementation();
std::unique_ptr<AlgorithmImplementation> CreateHmacImplementation();
std::unique_ptr<AlgorithmImplementation> CreateRsaOaepImplementation();
std::unique_ptr<AlgorithmImplementation> CreateRsaSsaImplementation();
std::unique_ptr<AlgorithmImplementation> CreateRsaPssImplementation();
std::unique_ptr<AlgorithmImplementation> CreateEcdsaImplementation();
std::unique_ptr<AlgorithmImplementation> CreateEcdhImplementation();
std::unique_ptr<AlgorithmImplementation> CreateHkdfImplementation();
std::unique_ptr<AlgorithmImplementation> CreatePbkdf2Implementation();
std::unique_ptr<AlgorithmImplementation> CreateEd25519Implementation();
std::unique_ptr<AlgorithmImplementation> CreateX25519Implementation();

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATIONS_H_
