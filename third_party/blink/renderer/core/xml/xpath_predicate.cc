/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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

#include "third_party/blink/renderer/core/xml/xpath_predicate.h"

#include <math.h>
#include "third_party/blink/renderer/core/xml/xpath_functions.h"
#include "third_party/blink/renderer/core/xml/xpath_util.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace xpath {

Number::Number(double value) : value_(value) {}

void Number::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
  Expression::Trace(visitor);
}

Value Number::Evaluate(EvaluationContext&) const {
  return value_;
}

StringExpression::StringExpression(const String& value) : value_(value) {}

void StringExpression::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
  Expression::Trace(visitor);
}

Value StringExpression::Evaluate(EvaluationContext&) const {
  return value_;
}

Value Negative::Evaluate(EvaluationContext& context) const {
  Value p(SubExpr(0)->Evaluate(context));
  return -p.ToNumber();
}

NumericOp::NumericOp(Opcode opcode, Expression* lhs, Expression* rhs)
    : opcode_(opcode) {
  AddSubExpression(lhs);
  AddSubExpression(rhs);
}

Value NumericOp::Evaluate(EvaluationContext& context) const {
  EvaluationContext cloned_context(context);
  Value lhs(SubExpr(0)->Evaluate(context));
  Value rhs(SubExpr(1)->Evaluate(cloned_context));

  double left_val = lhs.ToNumber();
  double right_val = rhs.ToNumber();

  switch (opcode_) {
    case kOP_Add:
      return left_val + right_val;
    case kOP_Sub:
      return left_val - right_val;
    case kOP_Mul:
      return left_val * right_val;
    case kOP_Div:
      return left_val / right_val;
    case kOP_Mod:
      return fmod(left_val, right_val);
  }
  NOTREACHED_IN_MIGRATION();
  return 0.0;
}

EqTestOp::EqTestOp(Opcode opcode, Expression* lhs, Expression* rhs)
    : opcode_(opcode) {
  AddSubExpression(lhs);
  AddSubExpression(rhs);
}

bool EqTestOp::Compare(EvaluationContext& context,
                       const Value& lhs,
                       const Value& rhs) const {
  if (lhs.IsNodeSet()) {
    const NodeSet& lhs_set = lhs.ToNodeSet(&context);
    if (rhs.IsNodeSet()) {
      // If both objects to be compared are node-sets, then the comparison
      // will be true if and only if there is a node in the first node-set
      // and a node in the second node-set such that the result of
      // performing the comparison on the string-values of the two nodes
      // is true.
      const NodeSet& rhs_set = rhs.ToNodeSet(&context);
      for (const auto& left_node : lhs_set) {
        for (const auto& right_node : rhs_set) {
          if (Compare(context, StringValue(left_node), StringValue(right_node)))
            return true;
        }
      }
      return false;
    }
    if (rhs.IsNumber()) {
      // If one object to be compared is a node-set and the other is a
      // number, then the comparison will be true if and only if there is
      // a node in the node-set such that the result of performing the
      // comparison on the number to be compared and on the result of
      // converting the string-value of that node to a number using the
      // number function is true.
      for (const auto& left_node : lhs_set) {
        if (Compare(context, Value(StringValue(left_node)).ToNumber(), rhs))
          return true;
      }
      return false;
    }
    if (rhs.IsString()) {
      // If one object to be compared is a node-set and the other is a
      // string, then the comparison will be true if and only if there is
      // a node in the node-set such that the result of performing the
      // comparison on the string-value of the node and the other string
      // is true.
      for (const auto& left_node : lhs_set) {
        if (Compare(context, StringValue(left_node), rhs))
          return true;
      }
      return false;
    }
    if (rhs.IsBoolean()) {
      // If one object to be compared is a node-set and the other is a
      // boolean, then the comparison will be true if and only if the
      // result of performing the comparison on the boolean and on the
      // result of converting the node-set to a boolean using the boolean
      // function is true.
      return Compare(context, lhs.ToBoolean(), rhs);
    }
    NOTREACHED_IN_MIGRATION();
  }
  if (rhs.IsNodeSet()) {
    const NodeSet& rhs_set = rhs.ToNodeSet(&context);
    if (lhs.IsNumber()) {
      for (const auto& right_node : rhs_set) {
        if (Compare(context, lhs, Value(StringValue(right_node)).ToNumber()))
          return true;
      }
      return false;
    }
    if (lhs.IsString()) {
      for (const auto& right_node : rhs_set) {
        if (Compare(context, lhs, StringValue(right_node)))
          return true;
      }
      return false;
    }
    if (lhs.IsBoolean())
      return Compare(context, lhs, rhs.ToBoolean());
    NOTREACHED_IN_MIGRATION();
  }

  // Neither side is a NodeSet.
  switch (opcode_) {
    case kOpcodeEqual:
    case kOpcodeNotEqual:
      bool equal;
      if (lhs.IsBoolean() || rhs.IsBoolean())
        equal = lhs.ToBoolean() == rhs.ToBoolean();
      else if (lhs.IsNumber() || rhs.IsNumber())
        equal = lhs.ToNumber() == rhs.ToNumber();
      else
        equal = lhs.ToString() == rhs.ToString();

      if (opcode_ == kOpcodeEqual)
        return equal;
      return !equal;
    case kOpcodeGreaterThan:
      return lhs.ToNumber() > rhs.ToNumber();
    case kOpcodeGreaterOrEqual:
      return lhs.ToNumber() >= rhs.ToNumber();
    case kOpcodeLessThan:
      return lhs.ToNumber() < rhs.ToNumber();
    case kOpcodeLessOrEqual:
      return lhs.ToNumber() <= rhs.ToNumber();
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

Value EqTestOp::Evaluate(EvaluationContext& context) const {
  EvaluationContext cloned_context(context);
  Value lhs(SubExpr(0)->Evaluate(context));
  Value rhs(SubExpr(1)->Evaluate(cloned_context));

  return Compare(context, lhs, rhs);
}

LogicalOp::LogicalOp(Opcode opcode, Expression* lhs, Expression* rhs)
    : opcode_(opcode) {
  AddSubExpression(lhs);
  AddSubExpression(rhs);
}

bool LogicalOp::ShortCircuitOn() const {
  return opcode_ != kOP_And;
}

Value LogicalOp::Evaluate(EvaluationContext& context) const {
  EvaluationContext cloned_context(context);
  Value lhs(SubExpr(0)->Evaluate(context));

  // This is not only an optimization, http://www.w3.org/TR/xpath
  // dictates that we must do short-circuit evaluation
  bool lhs_bool = lhs.ToBoolean();
  if (lhs_bool == ShortCircuitOn())
    return lhs_bool;

  return SubExpr(1)->Evaluate(cloned_context).ToBoolean();
}

Value Union::Evaluate(EvaluationContext& context) const {
  // SubExpr(0)->Evaluate() can change the context node, but SubExpr(1) should
  // start with the current context node.
  EvaluationContext cloned_context = context;
  Value lhs_result = SubExpr(0)->Evaluate(context);
  Value rhs = SubExpr(1)->Evaluate(cloned_context);

  NodeSet& result_set = lhs_result.ModifiableNodeSet(context);
  const NodeSet& rhs_nodes = rhs.ToNodeSet(&cloned_context);

  HeapHashSet<Member<Node>> nodes;
  for (const auto& node : result_set)
    nodes.insert(node);

  for (const auto& node : rhs_nodes) {
    if (nodes.insert(node).is_new_entry)
      result_set.Append(node);
  }

  // It is also possible to use merge sort to avoid making the result
  // unsorted; but this would waste the time in cases when order is not
  // important.
  result_set.MarkSorted(false);
  return lhs_result;
}

Predicate::Predicate(Expression* expr) : expr_(expr) {}

void Predicate::Trace(Visitor* visitor) const {
  visitor->Trace(expr_);
}

bool Predicate::Evaluate(EvaluationContext& context) const {
  DCHECK(expr_);

  // Apply a cloned context because position() requires the current
  // context node.
  EvaluationContext cloned_context = context;
  Value result(expr_->Evaluate(cloned_context));

  // foo[3] means foo[position()=3]
  if (result.IsNumber())
    return EqTestOp(EqTestOp::kOpcodeEqual, CreateFunction("position"),
                    MakeGarbageCollected<Number>(result.ToNumber()))
        .Evaluate(context)
        .ToBoolean();

  return result.ToBoolean();
}

}  // namespace xpath

}  // namespace blink
