/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/create_link_command.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

CreateLinkCommand::CreateLinkCommand(Document& document, const String& url)
    : CompositeEditCommand(document) {
  url_ = url;
}

void CreateLinkCommand::DoApply(EditingState* editing_state) {
  if (EndingSelection().IsNone())
    return;

  auto* anchor_element = MakeGarbageCollected<HTMLAnchorElement>(GetDocument());
  anchor_element->SetHref(AtomicString(url_));

  if (EndingSelection().IsRange()) {
    ApplyStyledElement(anchor_element, editing_state);
    if (editing_state->IsAborted())
      return;
  } else {
    InsertNodeAt(anchor_element, EndingVisibleSelection().Start(),
                 editing_state);
    if (editing_state->IsAborted())
      return;
    Text* text_node = Text::Create(GetDocument(), url_);
    AppendNode(text_node, anchor_element, editing_state);
    if (editing_state->IsAborted())
      return;
    SetEndingSelection(SelectionForUndoStep::From(
        SelectionInDOMTree::Builder()
            .Collapse(Position::InParentBeforeNode(*anchor_element))
            .Extend(Position::InParentAfterNode(*anchor_element))
            .Build()));
  }
}

}  // namespace blink
