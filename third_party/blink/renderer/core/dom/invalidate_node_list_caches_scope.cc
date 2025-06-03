// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/invalidate_node_list_caches_scope.h"

#include "base/logging.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"

namespace blink {

InvalidateNodeListCachesScope::InvalidateNodeListCachesScope(Document& document)
    : document_(document), invalidate_for_null_attr_name_(false) {
  if (document_.GetInvalidateNodeListCachesScope()) {
    return;
  }

  document_.SetInvalidateNodeListCachesScope(this);
}

InvalidateNodeListCachesScope::~InvalidateNodeListCachesScope() {
  if (document_.GetInvalidateNodeListCachesScope() != this) {
    return;
  }
  document_.SetInvalidateNodeListCachesScope(nullptr);

  if (invalidate_for_null_attr_name_) {
    document_.InvalidateNodeListCaches(nullptr);
  }
  for (const QualifiedName& attr_name : attr_names_) {
    document_.InvalidateNodeListCaches(&attr_name);
  }
}

// static
void InvalidateNodeListCachesScope::Invalidate(Document& document,
                                               const QualifiedName* attr_name) {
  auto* scope = document.GetInvalidateNodeListCachesScope();
  if (!scope) {
    document.InvalidateNodeListCaches(attr_name);
    return;
  }

  if (!attr_name) {
    scope->invalidate_for_null_attr_name_ = true;
    return;
  }

  scope->attr_names_.insert(*attr_name);
}

}  // namespace blink
