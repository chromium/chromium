// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class CORE_EXPORT StyleScope final : public GarbageCollected<StyleScope> {
 public:
  StyleScope(CSSSelectorList from, absl::optional<CSSSelectorList> to);
  StyleScope(const StyleScope&);

  void Trace(blink::Visitor* visitor) const { visitor->Trace(parent_); }

  StyleScope* CopyWithParent(const StyleScope*) const;

  const CSSSelectorList& From() const { return from_; }
  const absl::optional<CSSSelectorList>& To() const { return to_; }
  const StyleScope* Parent() const { return parent_.Get(); }

  // Specificity of the <scope-start> selector (::From()), plus the
  // specificity of the parent scope (if any).
  unsigned Specificity() const;

 private:
  CSSSelectorList from_;
  absl::optional<CSSSelectorList> to_;
  Member<const StyleScope> parent_;
  mutable absl::optional<unsigned> specificity_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_H_
