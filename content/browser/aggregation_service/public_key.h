// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Contains all the data of a public key.
struct CONTENT_EXPORT PublicKey {
 public:
  PublicKey(std::string id, std::vector<uint8_t> key);
  PublicKey(const PublicKey& other);
  PublicKey& operator=(const PublicKey& other);
  ~PublicKey();

  // String identifying the key, controlled by the helper server.
  std::string id;

  // The key itself.
  std::vector<uint8_t> key;
};

struct CONTENT_EXPORT PublicKeysForOrigin {
  PublicKeysForOrigin();
  PublicKeysForOrigin(url::Origin origin, std::vector<PublicKey> keys);
  PublicKeysForOrigin(const PublicKeysForOrigin& other);
  PublicKeysForOrigin& operator=(const PublicKeysForOrigin& other);
  ~PublicKeysForOrigin();

  url::Origin origin;
  std::vector<PublicKey> keys;
};

// Only used for logging.
CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const PublicKey& public_key);

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_
