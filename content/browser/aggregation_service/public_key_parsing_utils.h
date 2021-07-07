// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_PARSING_UTILS_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_PARSING_UTILS_H_

#include <vector>

#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"

namespace content {

namespace aggregation_service {

// Constructs a public key vector from multiple JSON arrays of key definition
// tagged with version ids. In case of an error or invalid JSON, returns an
// empty vector.
//
// The expected JSON schema is as follows.
//
// {
//   <Version ID> : [
//     {
//        "id" : <arbitrary string identifying the key, e.g. a UUID>,
//        "key" : <base64-encoded public key>,
//        "not_before" : <when key become valid, encoded as a string
//                      representation of an integer timestamp in milliseconds
//                      since the Unix epoch>,
//        "not_after" : <key expiry, encoded similarly to not_before>,
//     },
//     {  "id" : <different ID string>, ... },
//     ...
//   ]
// }
CONTENT_EXPORT std::vector<PublicKey> GetPublicKeys(base::Value& value);

}  // namespace aggregation_service

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_PARSING_UTILS_H_