/*
 * Copyright (c) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/replace_node_with_span_command.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

ReplaceNodeWithSpanCommand::ReplaceNodeWithSpanCommand(HTMLElement* element)
    : SimpleEditCommand(element->GetDocument()), element_to_replace_(element) {
  DCHECK(element_to_replace_);
}

static void SwapInNodePreservingAttributesAndChildren(
    HTMLElement* new_element,
    HTMLElement& element_to_replace) {
  DCHECK(element_to_replace.isConnected()) << element_to_replace;
  ContainerNode* parent_node = element_to_replace.parentNode();
  parent_node->InsertBefore(new_element, &element_to_replace);

  NodeVector children;
  GetChildNodes(element_to_replace, children);
  for (const auto& child : children)
    new_element->AppendChild(child);

  // FIXME: Fix this to send the proper MutationRecords when MutationObservers
  // are present.
  new_element->CloneAttributesFrom(element_to_replace);

  parent_node->RemoveChild(&element_to_replace, ASSERT_NO_EXCEPTION);
}

void ReplaceNodeWithSpanCommand::DoApply(EditingState*) {
  if (!element_to_replace_->isConnected())
    return;
  if (!span_element_) {
    span_element_ = MakeGarbageCollected<HTMLSpanElement>(
        element_to_replace_->GetDocument());
  }
  SwapInNodePreservingAttributesAndChildren(span_element_.Get(),
                                            *element_to_replace_);
}

void ReplaceNodeWithSpanCommand::DoUnapply() {
  if (!span_element_->isConnected())
    return;
  SwapInNodePreservingAttributesAndChildren(element_to_replace_.Get(),
                                            *span_element_);
}

void ReplaceNodeWithSpanCommand::Trace(Visitor* visitor) {
  visitor->Trace(element_to_replace_);
  visitor->Trace(span_element_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
