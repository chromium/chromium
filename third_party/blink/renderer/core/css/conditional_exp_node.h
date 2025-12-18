// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONDITIONAL_EXP_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONDITIONAL_EXP_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/kleene_value.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ConditionalExpNodeFunction;
class ConditionalExpNodeUnknown;
class MediaQueryFeatureExpNode;
class MediaQuerySet;
class NavigationExpNode;
class NavigationParamExpNode;

// Visitor and evaluation handler for leaf expression nodes, and contents of
// such leaves, and functions. Will visit in tree order. Ancestor compound
// expression nodes ("and" / "or" operators) may decide to short-circuit and
// skip expression subtrees based on the evaluation of the leftmost operand. To
// prevent this (i.e. for full traversal), return KleeneValue::kUnknown from the
// overrides.
class ConditionalExpNodeVisitor {
 public:
  virtual KleeneValue EvaluateNavigationExpNode(const NavigationExpNode&) {
    return KleeneValue::kUnknown;
  }
  virtual KleeneValue EvaluateNavigationParamExpNode(
      const NavigationParamExpNode&) {
    return KleeneValue::kUnknown;
  }
  virtual KleeneValue EvaluateMediaQueryFeatureExpNode(
      const MediaQueryFeatureExpNode&) {
    return KleeneValue::kUnknown;
  }
  virtual KleeneValue EvaluateMediaQuerySet(const MediaQuerySet&) {
    return KleeneValue::kUnknown;
  }
  virtual KleeneValue EvaluateUnknown(const ConditionalExpNodeUnknown&) {
    return KleeneValue::kUnknown;
  }
  virtual void EnterFunction(const ConditionalExpNodeFunction&) {}
};

// Forms a tree of expression nodes, to evaluate conditional queries such as
// media queries.
class CORE_EXPORT ConditionalExpNode
    : public GarbageCollected<ConditionalExpNode> {
 public:
  virtual ~ConditionalExpNode() = default;

  virtual void Trace(Visitor*) const {}

  // Evaluate this expression (and descendant expressions) and return the
  // result. ConditionalExpNodeVisitor will be invoked on leaf nodes.
  virtual KleeneValue Evaluate(ConditionalExpNodeVisitor&) const = 0;

  // Serialize the expression and descendant expressions.
  virtual void SerializeTo(StringBuilder&) const = 0;

  String Serialize() const;

  // These helper functions return nullptr if any argument is nullptr.
  static const ConditionalExpNode* Not(const ConditionalExpNode*);
  static const ConditionalExpNode* Nested(const ConditionalExpNode*);
  static const ConditionalExpNode* Function(const ConditionalExpNode*,
                                            const AtomicString& name);
  static const ConditionalExpNode* And(const ConditionalExpNode*,
                                       const ConditionalExpNode*);
  static const ConditionalExpNode* Or(const ConditionalExpNode*,
                                      const ConditionalExpNode*);
};

class CORE_EXPORT ConditionalExpNodeUnary : public ConditionalExpNode {
 public:
  explicit ConditionalExpNodeUnary(const ConditionalExpNode* operand)
      : operand_(operand) {}

  void Trace(Visitor*) const override;

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;

 protected:
  Member<const ConditionalExpNode> operand_;
};

class CORE_EXPORT ConditionalExpNodeCompound : public ConditionalExpNode {
 public:
  ConditionalExpNodeCompound(const ConditionalExpNode* left,
                             const ConditionalExpNode* right)
      : left_(left), right_(right) {}

  void Trace(Visitor*) const override;

 protected:
  Member<const ConditionalExpNode> left_;
  Member<const ConditionalExpNode> right_;
};

class CORE_EXPORT ConditionalExpNodeAnd : public ConditionalExpNodeCompound {
 public:
  ConditionalExpNodeAnd(const ConditionalExpNode* left,
                        const ConditionalExpNode* right)
      : ConditionalExpNodeCompound(left, right) {}

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;
};

class CORE_EXPORT ConditionalExpNodeOr : public ConditionalExpNodeCompound {
 public:
  ConditionalExpNodeOr(const ConditionalExpNode* left,
                       const ConditionalExpNode* right)
      : ConditionalExpNodeCompound(left, right) {}

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;
};

class CORE_EXPORT ConditionalExpNodeNot : public ConditionalExpNodeUnary {
 public:
  explicit ConditionalExpNodeNot(const ConditionalExpNode* operand)
      : ConditionalExpNodeUnary(operand) {}

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;
};

class CORE_EXPORT ConditionalExpNodeNested : public ConditionalExpNodeUnary {
 public:
  explicit ConditionalExpNodeNested(const ConditionalExpNode* operand)
      : ConditionalExpNodeUnary(operand) {}

  void SerializeTo(StringBuilder&) const override;
};

class CORE_EXPORT ConditionalExpNodeFunction : public ConditionalExpNodeUnary {
 public:
  explicit ConditionalExpNodeFunction(const ConditionalExpNode* operand,
                                      const AtomicString& name)
      : ConditionalExpNodeUnary(operand), name_(name) {}

  const AtomicString& GetName() const { return name_; }

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;

 private:
  AtomicString name_;
};

class CORE_EXPORT ConditionalExpNodeUnknown : public ConditionalExpNode {
 public:
  explicit ConditionalExpNodeUnknown(const String& string) : string_(string) {}

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;

 private:
  String string_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONDITIONAL_EXP_NODE_H_
