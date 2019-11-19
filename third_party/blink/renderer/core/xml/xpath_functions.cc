/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc.
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

#include "third_party/blink/renderer/core/xml/xpath_functions.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/xml/xpath_util.h"
#include "third_party/blink/renderer/core/xml/xpath_value.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#include <algorithm>
#include <limits>

namespace blink {
namespace xpath {

static inline bool IsWhitespace(UChar c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

#define DEFINE_FUNCTION_CREATOR(Class) \
  static Function* Create##Class() { return MakeGarbageCollected<Class>(); }

class Interval {
 public:
  static const int kInf = -1;

  Interval();
  Interval(int value);
  Interval(int min, int max);

  bool Contains(int value) const;

 private:
  int min_;
  int max_;
};

struct FunctionRec {
  typedef Function* (*FactoryFn)();
  FactoryFn factory_fn;
  Interval args;
};

static HashMap<String, FunctionRec>* g_function_map;

class FunLast final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }

 public:
  FunLast() { SetIsContextSizeSensitive(true); }
};

class FunPosition final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }

 public:
  FunPosition() { SetIsContextPositionSensitive(true); }
};

class FunCount final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }
};

class FunId final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNodeSetValue; }
};

class FunLocalName final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }

 public:
  FunLocalName() {
    SetIsContextNodeSensitive(true);
  }  // local-name() with no arguments uses context node.
};

class FunNamespaceURI final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }

 public:
  FunNamespaceURI() {
    SetIsContextNodeSensitive(true);
  }  // namespace-uri() with no arguments uses context node.
};

class FunName final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }

 public:
  FunName() {
    SetIsContextNodeSensitive(true);
  }  // name() with no arguments uses context node.
};

class FunString final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }

 public:
  FunString() {
    SetIsContextNodeSensitive(true);
  }  // string() with no arguments uses context node.
};

class FunConcat final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }
};

class FunStartsWith final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kBooleanValue; }
};

class FunContains final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kBooleanValue; }
};

class FunSubstringBefore final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }
};

class FunSubstringAfter final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }
};

class FunSubstring final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }
};

class FunStringLength final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }

 public:
  FunStringLength() {
    SetIsContextNodeSensitive(true);
  }  // string-length() with no arguments uses context node.
};

class FunNormalizeSpace final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }

 public:
  FunNormalizeSpace() {
    SetIsContextNodeSensitive(true);
  }  // normalize-space() with no arguments uses context node.
};

class FunTranslate final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kStringValue; }
};

class FunBoolean final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kBooleanValue; }
};

class FunNot final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kBooleanValue; }
};

class FunTrue final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kBooleanValue; }
};

class FunFalse final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kBooleanValue; }
};

class FunLang final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kBooleanValue; }

 public:
  FunLang() {
    SetIsContextNodeSensitive(true);
  }  // lang() always works on context node.
};

class FunNumber final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }

 public:
  FunNumber() {
    SetIsContextNodeSensitive(true);
  }  // number() with no arguments uses context node.
};

class FunSum final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }
};

class FunFloor final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }
};

class FunCeiling final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }
};

class FunRound final : public Function {
  Value Evaluate(EvaluationContext&) const override;
  Value::Type ResultType() const override { return Value::kNumberValue; }

 public:
  static double Round(double);
};

DEFINE_FUNCTION_CREATOR(FunLast)
DEFINE_FUNCTION_CREATOR(FunPosition)
DEFINE_FUNCTION_CREATOR(FunCount)
DEFINE_FUNCTION_CREATOR(FunId)
DEFINE_FUNCTION_CREATOR(FunLocalName)
DEFINE_FUNCTION_CREATOR(FunNamespaceURI)
DEFINE_FUNCTION_CREATOR(FunName)

DEFINE_FUNCTION_CREATOR(FunString)
DEFINE_FUNCTION_CREATOR(FunConcat)
DEFINE_FUNCTION_CREATOR(FunStartsWith)
DEFINE_FUNCTION_CREATOR(FunContains)
DEFINE_FUNCTION_CREATOR(FunSubstringBefore)
DEFINE_FUNCTION_CREATOR(FunSubstringAfter)
DEFINE_FUNCTION_CREATOR(FunSubstring)
DEFINE_FUNCTION_CREATOR(FunStringLength)
DEFINE_FUNCTION_CREATOR(FunNormalizeSpace)
DEFINE_FUNCTION_CREATOR(FunTranslate)

DEFINE_FUNCTION_CREATOR(FunBoolean)
DEFINE_FUNCTION_CREATOR(FunNot)
DEFINE_FUNCTION_CREATOR(FunTrue)
DEFINE_FUNCTION_CREATOR(FunFalse)
DEFINE_FUNCTION_CREATOR(FunLang)

DEFINE_FUNCTION_CREATOR(FunNumber)
DEFINE_FUNCTION_CREATOR(FunSum)
DEFINE_FUNCTION_CREATOR(FunFloor)
DEFINE_FUNCTION_CREATOR(FunCeiling)
DEFINE_FUNCTION_CREATOR(FunRound)

#undef DEFINE_FUNCTION_CREATOR

inline Interval::Interval() : min_(kInf), max_(kInf) {}

inline Interval::Interval(int value) : min_(value), max_(value) {}

inline Interval::Interval(int min, int max) : min_(min), max_(max) {}

inline bool Interval::Contains(int value) const {
  if (min_ == kInf && max_ == kInf)
    return true;

  if (min_ == kInf)
    return value <= max_;

  if (max_ == kInf)
    return value >= min_;

  return value >= min_ && value <= max_;
}

void Function::SetArguments(HeapVector<Member<Expression>>& args) {
  DCHECK(!SubExprCount());

  // Some functions use context node as implicit argument, so when explicit
  // arguments are added, they may no longer be context node sensitive.
  if (name_ != "lang" && !args.IsEmpty())
    SetIsContextNodeSensitive(false);

  for (Expression* arg : args)
    AddSubExpression(arg);
}

Value FunLast::Evaluate(EvaluationContext& context) const {
  return context.size;
}

Value FunPosition::Evaluate(EvaluationContext& context) const {
  return context.position;
}

Value FunId::Evaluate(EvaluationContext& context) const {
  Value a = Arg(0)->Evaluate(context);
  StringBuilder id_list;  // A whitespace-separated list of IDs

  if (a.IsNodeSet()) {
    for (const auto& node : a.ToNodeSet(&context)) {
      id_list.Append(StringValue(node));
      id_list.Append(' ');
    }
  } else {
    id_list.Append(a.ToString());
  }

  TreeScope& context_scope = context.node->GetTreeScope();
  NodeSet* result(NodeSet::Create());
  HeapHashSet<Member<Node>> result_set;

  unsigned start_pos = 0;
  unsigned length = id_list.length();
  while (true) {
    while (start_pos < length && IsWhitespace(id_list[start_pos]))
      ++start_pos;

    if (start_pos == length)
      break;

    unsigned end_pos = start_pos;
    while (end_pos < length && !IsWhitespace(id_list[end_pos]))
      ++end_pos;

    // If there are several nodes with the same id, id() should return the first
    // one.  In WebKit, getElementById behaves so, too, although its behavior in
    // this case is formally undefined.
    Node* node = context_scope.getElementById(
        AtomicString(id_list.Substring(start_pos, end_pos - start_pos)));
    if (node && result_set.insert(node).is_new_entry)
      result->Append(node);

    start_pos = end_pos;
  }

  result->MarkSorted(false);

  return Value(result, Value::kAdopt);
}

static inline String ExpandedNameLocalPart(Node* node) {
  // The local part of an XPath expanded-name matches DOM local name for most
  // node types, except for namespace nodes and processing instruction nodes.
  // But note that Blink does not support namespace nodes.
  switch (node->getNodeType()) {
    case Node::kElementNode:
      return To<Element>(node)->localName();
    case Node::kAttributeNode:
      return To<Attr>(node)->localName();
    case Node::kProcessingInstructionNode:
      return To<ProcessingInstruction>(node)->target();
    default:
      return String();
  }
}

static inline String ExpandedNamespaceURI(Node* node) {
  switch (node->getNodeType()) {
    case Node::kElementNode:
      return To<Element>(node)->namespaceURI();
    case Node::kAttributeNode:
      return To<Attr>(node)->namespaceURI();
    default:
      return String();
  }
}

static inline String ExpandedName(Node* node) {
  AtomicString prefix;

  switch (node->getNodeType()) {
    case Node::kElementNode:
      prefix = To<Element>(node)->prefix();
      break;
    case Node::kAttributeNode:
      prefix = To<Attr>(node)->prefix();
      break;
    default:
      break;
  }

  return prefix.IsEmpty() ? ExpandedNameLocalPart(node)
                          : prefix + ":" + ExpandedNameLocalPart(node);
}

Value FunLocalName::Evaluate(EvaluationContext& context) const {
  if (ArgCount() > 0) {
    Value a = Arg(0)->Evaluate(context);
    if (!a.IsNodeSet())
      return "";

    Node* node = a.ToNodeSet(&context).FirstNode();
    return node ? ExpandedNameLocalPart(node) : "";
  }

  return ExpandedNameLocalPart(context.node.Get());
}

Value FunNamespaceURI::Evaluate(EvaluationContext& context) const {
  if (ArgCount() > 0) {
    Value a = Arg(0)->Evaluate(context);
    if (!a.IsNodeSet())
      return "";

    Node* node = a.ToNodeSet(&context).FirstNode();
    return node ? ExpandedNamespaceURI(node) : "";
  }

  return ExpandedNamespaceURI(context.node.Get());
}

Value FunName::Evaluate(EvaluationContext& context) const {
  if (ArgCount() > 0) {
    Value a = Arg(0)->Evaluate(context);
    if (!a.IsNodeSet())
      return "";

    Node* node = a.ToNodeSet(&context).FirstNode();
    return node ? ExpandedName(node) : "";
  }

  return ExpandedName(context.node.Get());
}

Value FunCount::Evaluate(EvaluationContext& context) const {
  Value a = Arg(0)->Evaluate(context);

  return double(a.ToNodeSet(&context).size());
}

Value FunString::Evaluate(EvaluationContext& context) const {
  if (!ArgCount())
    return Value(context.node.Get()).ToString();
  return Arg(0)->Evaluate(context).ToString();
}

Value FunConcat::Evaluate(EvaluationContext& context) const {
  StringBuilder result;
  result.ReserveCapacity(1024);

  unsigned count = ArgCount();
  for (unsigned i = 0; i < count; ++i) {
    String str(Arg(i)->Evaluate(context).ToString());
    result.Append(str);
  }

  return result.ToString();
}

Value FunStartsWith::Evaluate(EvaluationContext& context) const {
  String s1 = Arg(0)->Evaluate(context).ToString();
  String s2 = Arg(1)->Evaluate(context).ToString();

  if (s2.IsEmpty())
    return true;

  return s1.StartsWith(s2);
}

Value FunContains::Evaluate(EvaluationContext& context) const {
  String s1 = Arg(0)->Evaluate(context).ToString();
  String s2 = Arg(1)->Evaluate(context).ToString();

  if (s2.IsEmpty())
    return true;

  return s1.Contains(s2) != 0;
}

Value FunSubstringBefore::Evaluate(EvaluationContext& context) const {
  String s1 = Arg(0)->Evaluate(context).ToString();
  String s2 = Arg(1)->Evaluate(context).ToString();

  if (s2.IsEmpty())
    return "";

  wtf_size_t i = s1.Find(s2);

  if (i == kNotFound)
    return "";

  return s1.Left(i);
}

Value FunSubstringAfter::Evaluate(EvaluationContext& context) const {
  String s1 = Arg(0)->Evaluate(context).ToString();
  String s2 = Arg(1)->Evaluate(context).ToString();

  wtf_size_t i = s1.Find(s2);
  if (i == kNotFound)
    return "";

  return s1.Substring(i + s2.length());
}

// Returns |value| clamped to the range [lo, hi].
// TODO(dominicc): Replace with std::clamp when C++17 is allowed
// per <https://chromium-cpp.appspot.com/>
static double Clamp(const double value, const double lo, const double hi) {
  return std::min(hi, std::max(lo, value));
}

// Computes the 1-based start and end (exclusive) string indices for
// substring. This is all the positions [1, maxLen (inclusive)] where
// start <= position < start + len
static std::pair<unsigned, unsigned> ComputeSubstringStartEnd(double start,
                                                              double len,
                                                              double max_len) {
  DCHECK(std::isfinite(max_len));
  const double end = start + len;
  if (std::isnan(start) || std::isnan(end))
    return std::make_pair(1, 1);
  // Neither start nor end are NaN, but may still be +/- Inf
  const double clamped_start = Clamp(start, 1, max_len + 1);
  const double clamped_end = Clamp(end, clamped_start, max_len + 1);
  return std::make_pair(static_cast<unsigned>(clamped_start),
                        static_cast<unsigned>(clamped_end));
}

// substring(string, number pos, number? len)
//
// Characters in string are indexed from 1. Numbers are doubles and
// substring is specified to work with IEEE-754 infinity, NaN, and
// XPath's bespoke rounding function, round.
//
// <https://www.w3.org/TR/xpath/#function-substring>
Value FunSubstring::Evaluate(EvaluationContext& context) const {
  String source_string = Arg(0)->Evaluate(context).ToString();
  const double pos = FunRound::Round(Arg(1)->Evaluate(context).ToNumber());
  const double len = ArgCount() == 3
                         ? FunRound::Round(Arg(2)->Evaluate(context).ToNumber())
                         : std::numeric_limits<double>::infinity();
  const auto bounds =
      ComputeSubstringStartEnd(pos, len, source_string.length());
  if (bounds.second <= bounds.first)
    return "";
  return source_string.Substring(bounds.first - 1,
                                 bounds.second - bounds.first);
}

Value FunStringLength::Evaluate(EvaluationContext& context) const {
  if (!ArgCount())
    return Value(context.node.Get()).ToString().length();
  return Arg(0)->Evaluate(context).ToString().length();
}

Value FunNormalizeSpace::Evaluate(EvaluationContext& context) const {
  if (!ArgCount()) {
    String s = Value(context.node.Get()).ToString();
    return s.SimplifyWhiteSpace();
  }

  String s = Arg(0)->Evaluate(context).ToString();
  return s.SimplifyWhiteSpace();
}

Value FunTranslate::Evaluate(EvaluationContext& context) const {
  String s1 = Arg(0)->Evaluate(context).ToString();
  String s2 = Arg(1)->Evaluate(context).ToString();
  String s3 = Arg(2)->Evaluate(context).ToString();
  StringBuilder result;

  for (unsigned i1 = 0; i1 < s1.length(); ++i1) {
    UChar ch = s1[i1];
    wtf_size_t i2 = s2.find(ch);

    if (i2 == kNotFound)
      result.Append(ch);
    else if (i2 < s3.length())
      result.Append(s3[i2]);
  }

  return result.ToString();
}

Value FunBoolean::Evaluate(EvaluationContext& context) const {
  return Arg(0)->Evaluate(context).ToBoolean();
}

Value FunNot::Evaluate(EvaluationContext& context) const {
  return !Arg(0)->Evaluate(context).ToBoolean();
}

Value FunTrue::Evaluate(EvaluationContext&) const {
  return true;
}

Value FunLang::Evaluate(EvaluationContext& context) const {
  String lang = Arg(0)->Evaluate(context).ToString();

  const Attribute* language_attribute = nullptr;
  Node* node = context.node.Get();
  while (node) {
    if (auto* element = DynamicTo<Element>(node))
      language_attribute = element->Attributes().Find(xml_names::kLangAttr);

    if (language_attribute)
      break;
    node = node->parentNode();
  }

  if (!language_attribute)
    return false;

  String lang_value = language_attribute->Value();
  while (true) {
    if (DeprecatedEqualIgnoringCase(lang_value, lang))
      return true;

    // Remove suffixes one by one.
    wtf_size_t index = lang_value.ReverseFind('-');
    if (index == kNotFound)
      break;
    lang_value = lang_value.Left(index);
  }

  return false;
}

Value FunFalse::Evaluate(EvaluationContext&) const {
  return false;
}

Value FunNumber::Evaluate(EvaluationContext& context) const {
  if (!ArgCount())
    return Value(context.node.Get()).ToNumber();
  return Arg(0)->Evaluate(context).ToNumber();
}

Value FunSum::Evaluate(EvaluationContext& context) const {
  Value a = Arg(0)->Evaluate(context);
  if (!a.IsNodeSet())
    return 0.0;

  double sum = 0.0;
  const NodeSet& nodes = a.ToNodeSet(&context);
  // To be really compliant, we should sort the node-set, as floating point
  // addition is not associative.  However, this is unlikely to ever become a
  // practical issue, and sorting is slow.

  for (const auto& node : nodes)
    sum += Value(StringValue(node)).ToNumber();

  return sum;
}

Value FunFloor::Evaluate(EvaluationContext& context) const {
  return floor(Arg(0)->Evaluate(context).ToNumber());
}

Value FunCeiling::Evaluate(EvaluationContext& context) const {
  return ceil(Arg(0)->Evaluate(context).ToNumber());
}

double FunRound::Round(double val) {
  if (!std::isnan(val) && !std::isinf(val)) {
    if (std::signbit(val) && val >= -0.5)
      val *= 0;  // negative zero
    else
      val = floor(val + 0.5);
  }
  return val;
}

Value FunRound::Evaluate(EvaluationContext& context) const {
  return Round(Arg(0)->Evaluate(context).ToNumber());
}

struct FunctionMapping {
  const char* name;
  FunctionRec function;
};

static void CreateFunctionMap() {
  DCHECK(!g_function_map);
  const FunctionMapping functions[] = {
      {"boolean", {&CreateFunBoolean, 1}},
      {"ceiling", {&CreateFunCeiling, 1}},
      {"concat", {&CreateFunConcat, Interval(2, Interval::kInf)}},
      {"contains", {&CreateFunContains, 2}},
      {"count", {&CreateFunCount, 1}},
      {"false", {&CreateFunFalse, 0}},
      {"floor", {&CreateFunFloor, 1}},
      {"id", {&CreateFunId, 1}},
      {"lang", {&CreateFunLang, 1}},
      {"last", {&CreateFunLast, 0}},
      {"local-name", {&CreateFunLocalName, Interval(0, 1)}},
      {"name", {&CreateFunName, Interval(0, 1)}},
      {"namespace-uri", {&CreateFunNamespaceURI, Interval(0, 1)}},
      {"normalize-space", {&CreateFunNormalizeSpace, Interval(0, 1)}},
      {"not", {&CreateFunNot, 1}},
      {"number", {&CreateFunNumber, Interval(0, 1)}},
      {"position", {&CreateFunPosition, 0}},
      {"round", {&CreateFunRound, 1}},
      {"starts-with", {&CreateFunStartsWith, 2}},
      {"string", {&CreateFunString, Interval(0, 1)}},
      {"string-length", {&CreateFunStringLength, Interval(0, 1)}},
      {"substring", {&CreateFunSubstring, Interval(2, 3)}},
      {"substring-after", {&CreateFunSubstringAfter, 2}},
      {"substring-before", {&CreateFunSubstringBefore, 2}},
      {"sum", {&CreateFunSum, 1}},
      {"translate", {&CreateFunTranslate, 3}},
      {"true", {&CreateFunTrue, 0}},
  };

  g_function_map = new HashMap<String, FunctionRec>;
  for (size_t i = 0; i < base::size(functions); ++i)
    g_function_map->Set(functions[i].name, functions[i].function);
}

Function* CreateFunction(const String& name) {
  HeapVector<Member<Expression>> args;
  return CreateFunction(name, args);
}

Function* CreateFunction(const String& name,
                         HeapVector<Member<Expression>>& args) {
  if (!g_function_map)
    CreateFunctionMap();

  HashMap<String, FunctionRec>::iterator function_map_iter =
      g_function_map->find(name);
  FunctionRec* function_rec = nullptr;

  if (function_map_iter == g_function_map->end() ||
      !(function_rec = &function_map_iter->value)->args.Contains(args.size()))
    return nullptr;

  Function* function = function_rec->factory_fn();
  function->SetArguments(args);
  function->SetName(name);
  return function;
}

}  // namespace xpath
}  // namespace blink
