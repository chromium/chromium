// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {
class StyleSheetContents;

class CORE_EXPORT StyleScope final : public GarbageCollected<StyleScope> {
 public:
  StyleScope(CSSSelectorList* from, CSSSelectorList* to);
  StyleScope(const StyleScope&);
  static StyleScope* Parse(CSSParserTokenRange prelude,
                           const CSSParserContext* context,
                           StyleSheetContents* style_sheet);

  void Trace(blink::Visitor* visitor) const {
    visitor->Trace(from_);
    visitor->Trace(to_);
    visitor->Trace(parent_);
  }

  StyleScope* CopyWithParent(const StyleScope*) const;

  const CSSSelectorList& From() const { return *from_; }
  const CSSSelectorList* To() const { return to_.Get(); }  // May be nullptr.
  const StyleScope* Parent() const { return parent_.Get(); }

  // Specificity of the <scope-start> selector (::From()), plus the
  // specificity of the parent scope (if any).
  unsigned Specificity() const;

 private:
  Member<CSSSelectorList> from_;
  Member<CSSSelectorList> to_;  // May be nullptr.
  Member<const StyleScope> parent_;
  mutable absl::optional<unsigned> specificity_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_H_
