/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/xpath_evaluator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/xml/xpath_expression.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/core/xml/xpath_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

XPathExpression* XPathEvaluator::createExpression(
    ExecutionContext* execution_context,
    const String& expression,
    V8XPathNSResolver* resolver,
    ExceptionState& exception_state) {
  return XPathExpression::CreateExpression(expression, resolver,
                                           execution_context, exception_state);
}

Node* XPathEvaluator::createNSResolver(Node* node_resolver) {
  // https://dom.spec.whatwg.org/#dom-xpathevaluatorbase-creatensresolver
  // The createNSResolver(nodeResolver) method steps are to return nodeResolver.
  return node_resolver;
}

XPathResult* XPathEvaluator::evaluate(ExecutionContext* execution_context,
                                      const String& expression,
                                      Node* context_node,
                                      V8XPathNSResolver* resolver,
                                      uint16_t type,
                                      const ScriptValue&,
                                      ExceptionState& exception_state) {
  if (!xpath::IsValidContextNode(context_node)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The node provided is '" + context_node->nodeName() +
            "', which is not a valid context node type.");
    return nullptr;
  }

  XPathExpression* expr = createExpression(execution_context, expression,
                                           resolver, exception_state);
  if (exception_state.HadException())
    return nullptr;

  return expr->evaluate(execution_context, context_node, type, ScriptValue(),
                        exception_state);
}

}  // namespace blink
