// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_BLOB_BLOB_URL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_BLOB_BLOB_URL_H_

#include "third_party/blink/public/common/common_export.h"
#include "url/gurl.h"

namespace url {
class Origin;
}

namespace blink {

// Creates a blob URL from `origin`. If `security_origin_serializes_as_null` is
// true, `origin` will be serialized to `null` regardless of whether it is
// opaque. See `blink::SecurityOrigin::ToString()` for why this is necessary.
BLINK_COMMON_EXPORT GURL CreateBlobUrl(const url::Origin& origin,
                                       bool security_origin_serializes_as_null);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_BLOB_BLOB_URL_H_
