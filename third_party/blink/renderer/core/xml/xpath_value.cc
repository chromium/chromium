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

#include "third_party/blink/renderer/core/xml/xpath_value.h"

#include <limits>
#include "third_party/blink/renderer/core/xml/xpath_expression_node.h"
#include "third_party/blink/renderer/core/xml/xpath_util.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {
namespace xpath {

const Value::AdoptTag Value::kAdopt = {};

void ValueData::Trace(Visitor* visitor) const {
  visitor->Trace(node_set_);
}

void Value::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
}

const NodeSet& Value::ToNodeSet(EvaluationContext* context) const {
  if (!IsNodeSet() && context)
    context->had_type_conversion_error = true;

  if (!data_) {
    DEFINE_STATIC_LOCAL(Persistent<NodeSet>, empty_node_set,
                        (NodeSet::Create()));
    return *empty_node_set;
  }

  return data_->GetNodeSet();
}

NodeSet& Value::ModifiableNodeSet(EvaluationContext& context) {
  if (!IsNodeSet())
    context.had_type_conversion_error = true;

  if (!data_)
    data_ = MakeGarbageCollected<ValueData>();

  type_ = kNodeSetValue;
  return data_->GetNodeSet();
}

bool Value::ToBoolean() const {
  switch (type_) {
    case kNodeSetValue:
      return !data_->GetNodeSet().IsEmpty();
    case kBooleanValue:
      return bool_;
    case kNumberValue:
      return number_ && !std::isnan(number_);
    case kStringValue:
      return !data_->string_.empty();
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

double Value::ToNumber() const {
  switch (type_) {
    case kNodeSetValue:
      return Value(ToString()).ToNumber();
    case kNumberValue:
      return number_;
    case kStringValue: {
      const String& str = data_->string_.SimplifyWhiteSpace();

      // String::toDouble() supports exponential notation, which is not
      // allowed in XPath.
      unsigned len = str.length();
      for (unsigned i = 0; i < len; ++i) {
        UChar c = str[i];
        if (!IsASCIIDigit(c) && c != '.' && c != '-')
          return std::numeric_limits<double>::quiet_NaN();
      }

      bool can_convert;
      double value = str.ToDouble(&can_convert);
      if (can_convert)
        return value;
      return std::numeric_limits<double>::quiet_NaN();
    }
    case kBooleanValue:
      return bool_;
  }
  NOTREACHED_IN_MIGRATION();
  return 0.0;
}

String Value::ToString() const {
  switch (type_) {
    case kNodeSetValue:
      if (data_->GetNodeSet().IsEmpty())
        return "";
      return StringValue(data_->GetNodeSet().FirstNode());
    case kStringValue:
      return data_->string_;
    case kNumberValue:
      if (std::isnan(number_))
        return "NaN";
      if (number_ == 0)
        return "0";
      if (std::isinf(number_))
        return std::signbit(number_) ? "-Infinity" : "Infinity";
      return String::Number(number_);
    case kBooleanValue:
      return bool_ ? "true" : "false";
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

}  // namespace xpath
}  // namespace blink
