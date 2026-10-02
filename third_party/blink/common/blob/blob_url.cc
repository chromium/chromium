// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/blob/blob_url.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "url/origin.h"

namespace blink {

GURL CreateBlobUrl(const url::Origin& origin,
                   bool security_origin_serializes_as_null) {
  const std::string serialized_origin =
      security_origin_serializes_as_null ? "null" : origin.Serialize();
  return GURL(
      base::StrCat({"blob:", serialized_origin, "/",
                    base::Uuid::GenerateRandomV4().AsLowercaseString()}));
}

}  // namespace blink
