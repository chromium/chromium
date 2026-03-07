// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_AD_TAGGING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_AD_TAGGING_UTILS_H_

#include <variant>

#include "base/types/strong_alias.h"
#include "components/subresource_filter/core/common/scoped_rule.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

using V8ScriptId = base::StrongAlias<class V8ScriptIdTag, int>;

template <>
struct HashTraits<V8ScriptId> : GenericHashTraits<V8ScriptId> {
  static unsigned GetHash(const V8ScriptId& value) {
    return blink::HashInt(value.value());
  }
  static constexpr bool kEmptyValueIsZero = true;
  static V8ScriptId EmptyValue() { return V8ScriptId(0); }
  static void ConstructDeletedValue(V8ScriptId& slot) { slot = V8ScriptId(-1); }
  static bool IsDeletedValue(const V8ScriptId& value) {
    return value == V8ScriptId(-1);
  }
};

struct NoProvenance {};

// Represents the reason why a resource (e.g., a script or image) is classified
// as an ad. It can be one of:
// - NoProvenance: The resource has neither an ancestor nor a rule match.
// - subresource_filter::ScopedRule: The resource URL (or a previous URL in its
//   redirect chain) is flagged by the subresource filter.
// - V8ScriptId: The resource itself is not flagged, but another ad script (the
//   "ancestor") exists in its creation stack.
using AdProvenance =
    std::variant<NoProvenance, subresource_filter::ScopedRule, V8ScriptId>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_AD_TAGGING_UTILS_H_
