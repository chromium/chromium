/*
 * Copyright (C) 2013 Google, Inc.
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

#include "third_party/blink/renderer/core/xml/document_xpath_evaluator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/xml/xpath_expression.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
const char DocumentXPathEvaluator::kSupplementName[] = "DocumentXPathEvaluator";

DocumentXPathEvaluator::DocumentXPathEvaluator(Document& document)
    : Supplement<Document>(document) {}

DocumentXPathEvaluator& DocumentXPathEvaluator::From(Document& document) {
  DocumentXPathEvaluator* cache =
      Supplement<Document>::From<DocumentXPathEvaluator>(document);
  if (!cache) {
    cache = MakeGarbageCollected<DocumentXPathEvaluator>(document);
    Supplement<Document>::ProvideTo(document, cache);
  }
  return *cache;
}

XPathExpression* DocumentXPathEvaluator::createExpression(
    Document& document,
    const String& expression,
    V8XPathNSResolver* resolver,
    ExceptionState& exception_state) {
  DocumentXPathEvaluator& suplement = From(document);
  if (!suplement.xpath_evaluator_)
    suplement.xpath_evaluator_ = XPathEvaluator::Create();
  return suplement.xpath_evaluator_->createExpression(
      document.GetExecutionContext(), expression, resolver, exception_state);
}

Node* DocumentXPathEvaluator::createNSResolver(Document& document,
                                               Node* node_resolver) {
  DocumentXPathEvaluator& suplement = From(document);
  if (!suplement.xpath_evaluator_)
    suplement.xpath_evaluator_ = XPathEvaluator::Create();
  return suplement.xpath_evaluator_->createNSResolver(node_resolver);
}

XPathResult* DocumentXPathEvaluator::evaluate(Document& document,
                                              const String& expression,
                                              Node* context_node,
                                              V8XPathNSResolver* resolver,
                                              uint16_t type,
                                              const ScriptValue&,
                                              ExceptionState& exception_state) {
  DocumentXPathEvaluator& suplement = From(document);
  if (!suplement.xpath_evaluator_)
    suplement.xpath_evaluator_ = XPathEvaluator::Create();
  return suplement.xpath_evaluator_->evaluate(
      document.GetExecutionContext(), expression, context_node, resolver, type,
      ScriptValue(), exception_state);
}

void DocumentXPathEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(xpath_evaluator_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
