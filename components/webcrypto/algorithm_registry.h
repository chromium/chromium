// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHM_REGISTRY_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHM_REGISTRY_H_

#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

class AlgorithmImplementation;
class Status;

// Retrieves the AlgorithmImplementation applicable for |id|.
//
// If there is no available implementation, then an error is returned, and
// *impl is set to NULL.
//
// Otherwise Success is returned and *impl is set to a non-NULL value. The
// AlgorithmImplementation pointer will remain valid until the program's
// termination.
Status GetAlgorithmImplementation(blink::WebCryptoAlgorithmId id,
                                  const AlgorithmImplementation** impl);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHM_REGISTRY_H_
