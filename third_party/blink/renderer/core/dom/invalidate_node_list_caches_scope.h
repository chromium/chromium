// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INVALIDATE_NODE_LIST_CACHES_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INVALIDATE_NODE_LIST_CACHES_SCOPE_H_

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class Document;

// InvalidateNodeListCachesScope is the stack-allocated scoping class
// that defers calls to the 'Document::InvalidateNodeListCaches()' method
// until the end of the lifetime of the first-allocated scoping instance,
// to avoid n^2 calls to 'LiveNodeListBase::InvalidateCacheForAttribute()'
// while cloning an element containing form elements inserted into a form
// collection. (crbug.com/40874584)
class InvalidateNodeListCachesScope {
  STACK_ALLOCATED();

 public:
  explicit InvalidateNodeListCachesScope(Document& document);
  ~InvalidateNodeListCachesScope();
  InvalidateNodeListCachesScope(const InvalidateNodeListCachesScope&) = delete;
  InvalidateNodeListCachesScope& operator=(
      const InvalidateNodeListCachesScope&) = delete;

  static void Invalidate(Document& document, const QualifiedName* attr_name);

 private:
  Document& document_;
  bool invalidate_for_null_attr_name_;
  HashSet<QualifiedName> attr_names_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INVALIDATE_NODE_LIST_CACHES_SCOPE_H_
