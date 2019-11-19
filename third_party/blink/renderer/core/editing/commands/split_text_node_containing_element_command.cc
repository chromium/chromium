/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/commands/split_text_node_containing_element_command.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

SplitTextNodeContainingElementCommand::SplitTextNodeContainingElementCommand(
    Text* text,
    int offset)
    : CompositeEditCommand(text->GetDocument()), text_(text), offset_(offset) {
  DCHECK(text_);
  DCHECK_GT(text_->length(), 0u);
}

void SplitTextNodeContainingElementCommand::DoApply(EditingState*) {
  DCHECK(text_);
  DCHECK_GT(offset_, 0);

  SplitTextNode(text_.Get(), offset_);

  Element* parent = text_->parentElement();
  if (!parent || !parent->parentElement() ||
      !HasEditableStyle(*parent->parentElement()))
    return;

  LayoutObject* parent_layout_object = parent->GetLayoutObject();
  if (!parent_layout_object || !parent_layout_object->IsInline()) {
    WrapContentsInDummySpan(parent);
    auto* first_child_element = DynamicTo<Element>(parent->firstChild());
    if (!first_child_element)
      return;
    parent = first_child_element;
  }

  SplitElement(parent, text_.Get());
}

void SplitTextNodeContainingElementCommand::Trace(Visitor* visitor) {
  visitor->Trace(text_);
  CompositeEditCommand::Trace(visitor);
}

}  // namespace blink
