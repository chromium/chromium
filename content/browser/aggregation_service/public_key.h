// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Class that contains all the data of a public key.
class CONTENT_EXPORT PublicKey {
 public:
  PublicKey() = default;
  PublicKey(std::string id,
            std::string key,
            base::Time not_before_time,
            base::Time not_after_time);
  PublicKey(const PublicKey& other) = default;
  PublicKey& operator=(const PublicKey& other) = default;
  ~PublicKey() = default;

  std::string id() const { return id_; }

  std::string key() const { return key_; }

  base::Time not_before_time() const { return not_before_time_; }

  base::Time not_after_time() const { return not_after_time_; }

 private:
  // String identifying the key, controlled by the helper server.
  std::string id_;

  // Base64-encoded public key.
  std::string key_;

  // The first time the key is valid.
  base::Time not_before_time_;

  // The time the key expires.
  base::Time not_after_time_;
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
