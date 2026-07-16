// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RANGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// NodeRange is the spec-proposed intermediate interface between AbstractRange
// and Range/StaticRange that owns startContainer/endContainer:
// https://github.com/whatwg/dom/pull/1470
class CORE_EXPORT NodeRange : public AbstractRange {
  DEFINE_WRAPPERTYPEINFO();

 protected:
  NodeRange();
  ~NodeRange() override;
};

template <>
struct DowncastTraits<NodeRange> {
  static bool AllowFrom(const AbstractRange& abstract_range) {
    // The only AbstractRange subtypes are NodeRange and OpaqueRange.
    return !abstract_range.IsOpaqueRange();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RANGE_H_
