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

#include "third_party/blink/renderer/core/editing/commands/wrap_contents_in_dummy_span_command.h"

#include "third_party/blink/renderer/core/editing/commands/apply_style_command.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

WrapContentsInDummySpanCommand::WrapContentsInDummySpanCommand(Element* element)
    : SimpleEditCommand(element->GetDocument()), element_(element) {
  DCHECK(element_);
}

void WrapContentsInDummySpanCommand::ExecuteApply() {
  NodeVector children;
  GetChildNodes(*element_, children);

  for (auto& child : children)
    dummy_span_->AppendChild(child.Release(), IGNORE_EXCEPTION_FOR_TESTING);

  element_->AppendChild(dummy_span_.Get(), IGNORE_EXCEPTION_FOR_TESTING);
}

void WrapContentsInDummySpanCommand::DoApply(EditingState*) {
  dummy_span_ = MakeGarbageCollected<HTMLSpanElement>(GetDocument());

  ExecuteApply();
}

void WrapContentsInDummySpanCommand::DoUnapply() {
  DCHECK(element_);

  if (!dummy_span_ || !HasEditableStyle(*element_))
    return;

  NodeVector children;
  GetChildNodes(*dummy_span_, children);

  for (auto& child : children)
    element_->AppendChild(child.Release(), IGNORE_EXCEPTION_FOR_TESTING);

  dummy_span_->remove(IGNORE_EXCEPTION_FOR_TESTING);
}

void WrapContentsInDummySpanCommand::DoReapply() {
  DCHECK(element_);

  if (!dummy_span_ || !HasEditableStyle(*element_))
    return;

  ExecuteApply();
}

void WrapContentsInDummySpanCommand::Trace(Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(dummy_span_);
  SimpleEditCommand::Trace(visitor);
}

}  // namespace blink
