/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CONTENT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CONTENT_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT HTMLContentElement final : public V0InsertionPoint {
  DEFINE_WRAPPERTYPEINFO();

 public:
  HTMLContentElement(Document&);
  ~HTMLContentElement() override;

  bool CanAffectSelector() const override { return true; }

  bool CanSelectNode(const HeapVector<Member<Node>, 32>& siblings,
                     int nth) const;

  const CSSSelectorList& SelectorList() const;
  bool IsSelectValid() const;

  void Trace(Visitor*) override;

 private:
  void ParseAttribute(const AttributeModificationParams&) override;

  bool ValidateSelect() const;
  void ParseSelect();

  bool MatchSelector(Element&) const;

  bool should_parse_select_;
  bool is_valid_selector_;
  AtomicString select_;
  CSSSelectorList selector_list_;
};

inline const CSSSelectorList& HTMLContentElement::SelectorList() const {
  if (should_parse_select_)
    const_cast<HTMLContentElement*>(this)->ParseSelect();
  return selector_list_;
}

inline bool HTMLContentElement::IsSelectValid() const {
  if (should_parse_select_)
    const_cast<HTMLContentElement*>(this)->ParseSelect();
  return is_valid_selector_;
}

inline bool HTMLContentElement::CanSelectNode(
    const HeapVector<Member<Node>, 32>& siblings,
    int nth) const {
  if (select_.IsNull() || select_.IsEmpty())
    return true;
  if (!IsSelectValid())
    return false;
  auto* element = DynamicTo<Element>(siblings[nth].Get());
  if (!element)
    return false;
  return MatchSelector(*element);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CONTENT_ELEMENT_H_
