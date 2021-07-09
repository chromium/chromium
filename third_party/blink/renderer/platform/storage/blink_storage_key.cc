// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

namespace blink {

BlinkStorageKey::BlinkStorageKey()
    : origin_(SecurityOrigin::CreateUniqueOpaque()) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin)
    : origin_(std::move(origin)) {
  DCHECK(origin_);
}

String BlinkStorageKey::ToDebugString() const {
  return "{ origin: " + GetSecurityOrigin()->ToString() + " }";
}

bool operator==(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  DCHECK(lhs.GetSecurityOrigin());
  DCHECK(rhs.GetSecurityOrigin());
  return lhs.GetSecurityOrigin()->IsSameOriginWith(
      rhs.GetSecurityOrigin().get());
}

bool operator!=(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const BlinkStorageKey& key) {
  return ostream << key.ToDebugString();
}

}  // namespace blink
