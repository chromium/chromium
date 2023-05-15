// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/uuid.h"

namespace blink {

SessionStorageNamespaceId AllocateSessionStorageNamespaceId() {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::replace(guid.begin(), guid.end(), '-', '_');
  // The database deserialization code makes assumptions based on this length.
  DCHECK_EQ(guid.size(), kSessionStorageNamespaceIdLength);
  return guid;
}

}  // namespace blink
