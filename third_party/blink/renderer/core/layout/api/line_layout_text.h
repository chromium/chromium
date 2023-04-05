// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_TEXT_H_

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class LayoutText;

class LineLayoutText : public LineLayoutItem {
 public:
  explicit LineLayoutText(LayoutText* layout_object)
      : LineLayoutItem(layout_object) {}

  explicit LineLayoutText(const LineLayoutItem& item) : LineLayoutItem(item) {
    SECURITY_DCHECK(!item || item.IsText());
  }

  explicit LineLayoutText(std::nullptr_t) : LineLayoutItem(nullptr) {}

  LineLayoutText() = default;

  bool IsWordBreak() const { return ToText()->IsWordBreak(); }

  bool IsAllCollapsibleWhitespace() const {
    return ToText()->IsAllCollapsibleWhitespace();
  }

  OnlyWhitespaceOrNbsp ContainsOnlyWhitespaceOrNbsp() const {
    return ToText()->ContainsOnlyWhitespaceOrNbsp();
  }

  UChar CharacterAt(unsigned offset) const {
    return ToText()->CharacterAt(offset);
  }

  UChar UncheckedCharacterAt(unsigned offset) const {
    return ToText()->UncheckedCharacterAt(offset);
  }

  UChar32 CodepointAt(unsigned offset) const {
    return ToText()->CodepointAt(offset);
  }

  bool Is8Bit() const { return ToText()->Is8Bit(); }

  const LChar* Characters8() const { return ToText()->Characters8(); }

  const UChar* Characters16() const { return ToText()->Characters16(); }

  bool HasEmptyText() const { return ToText()->HasEmptyText(); }

  unsigned TextLength() const { return ToText()->TextLength(); }

  unsigned ResolvedTextLength() const { return ToText()->ResolvedTextLength(); }

  const String& GetText() const { return ToText()->GetText(); }

  bool ContainsOnlyWhitespace(unsigned from, unsigned len) const {
    return ToText()->ContainsOnlyWhitespace(from, len);
  }

  float HyphenWidth(const Font& font, TextDirection text_direction) {
    return ToText()->HyphenWidth(font, text_direction);
  }

  unsigned TextStartOffset() const { return ToText()->TextStartOffset(); }

  UChar PreviousCharacter() const { return ToText()->PreviousCharacter(); }

  LayoutTextSelectionStatus SelectionStatus() const;

 private:
  LayoutText* ToText() { return To<LayoutText>(GetLayoutObject()); }
  const LayoutText* ToText() const { return To<LayoutText>(GetLayoutObject()); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_TEXT_H_
