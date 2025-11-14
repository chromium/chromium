// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/testing/internals_content_extraction.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

AIPageContentAgent* GetAIPageContentAgent(Document& document,
                                          ExceptionState& exception_state) {
  if (!document.GetFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document provided is invalid.");
    return nullptr;
  }

  LocalFrame* frame = DynamicTo<LocalFrame>(document.GetFrame());
  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document must be in a local frame.");
    return nullptr;
  }

  // AiPageContentAgent is a Supplement on Document.
  AIPageContentAgent* agent =
      AIPageContentAgent::GetOrCreateForTesting(document);
  if (!agent) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "No AIPageContentAgent");
    return nullptr;
  }
  return agent;
}

}  // namespace

String InternalsContentExtraction::dumpContentNodeTree(
    Internals&,
    Document* document,
    ExceptionState& exception_state) {
  DCHECK(document);
  AIPageContentAgent* agent = GetAIPageContentAgent(*document, exception_state);
  if (!agent) {
    return String();
  }
  return agent->DumpContentNodeTreeForTest();
}

String InternalsContentExtraction::dumpContentNode(
    Internals&,
    Node* node,
    ExceptionState& exception_state) {
  DCHECK(node);
  AIPageContentAgent* agent =
      GetAIPageContentAgent(node->GetDocument(), exception_state);
  if (!agent) {
    return String();
  }
  return agent->DumpContentNodeForTest(node);
}

}  // namespace blink
