/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/xml/xpath_expression.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_xpath_ns_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/xml/xpath_expression_node.h"
#include "third_party/blink/renderer/core/xml/xpath_parser.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/core/xml/xpath_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

XPathExpression::XPathExpression() = default;

XPathExpression* XPathExpression::CreateExpression(
    const String& expression,
    V8XPathNSResolver* resolver,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  auto* expr = MakeGarbageCollected<XPathExpression>();
  xpath::Parser parser(execution_context);

  expr->top_expression_ =
      parser.ParseStatement(expression, resolver, exception_state);
  if (!expr->top_expression_)
    return nullptr;

  return expr;
}

void XPathExpression::Trace(Visitor* visitor) const {
  visitor->Trace(top_expression_);
  ScriptWrappable::Trace(visitor);
}

XPathResult* XPathExpression::evaluate(ExecutionContext* execution_context,
                                       Node* context_node,
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

  bool had_type_conversion_error = false;
  xpath::EvaluationContext evaluation_context(*context_node,
                                              had_type_conversion_error);
  evaluation_context.use_counter = execution_context;
  auto* result = MakeGarbageCollected<XPathResult>(
      evaluation_context, top_expression_->Evaluate(evaluation_context));

  if (had_type_conversion_error) {
    // It is not specified what to do if type conversion fails while evaluating
    // an expression.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Type conversion failed while evaluating the expression.");
    return nullptr;
  }

  if (type != XPathResult::kAnyType) {
    result->ConvertTo(type, exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  return result;
}

}  // namespace blink
