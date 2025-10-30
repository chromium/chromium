// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONDITIONAL_EXP_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONDITIONAL_EXP_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/kleene_value.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaQueryExp;
class MediaQueryFeatureExpNode;
class MediaQuerySet;

// Evaluation handler for leaf expression nodes, such as media feature
// expression nodes, or style() functions.
class ConditionalLeafExpressionHandler {
 public:
  virtual KleeneValue EvaluateMediaQueryFeatureExpNode(
      const MediaQueryFeatureExpNode&) {
    return KleeneValue::kFalse;
  }
  virtual KleeneValue EvaluateMediaQuerySet(const MediaQuerySet&) {
    return KleeneValue::kFalse;
  }
};

// Forms a tree of expression nodes, to evaluate conditional queries such as
// media queries.
class CORE_EXPORT ConditionalExpNode
    : public GarbageCollected<ConditionalExpNode> {
 public:
  virtual ~ConditionalExpNode() = default;

  virtual void Trace(Visitor*) const {}

  // Evaluate this expression (and descendant expressions) and return the
  // result.
  virtual KleeneValue Evaluate(ConditionalLeafExpressionHandler&) const = 0;

  // Serialize the expression and descendant expressions.
  virtual void SerializeTo(StringBuilder&) const = 0;

  // Only used inside media queries.
  virtual void CollectExpressions(HeapVector<MediaQueryExp>&) const {
    NOTREACHED();
  }

  // Only used inside container queries.
  enum FeatureFlag {
    kFeatureUnknown = 1 << 1,
    kFeatureWidth = 1 << 2,
    kFeatureHeight = 1 << 3,
    kFeatureInlineSize = 1 << 4,
    kFeatureBlockSize = 1 << 5,
    kFeatureStyle = 1 << 6,
    kFeatureSticky = 1 << 7,
    kFeatureSnap = 1 << 8,
    kFeatureScrollable = 1 << 9,
    kFeatureScrolled = 1 << 10,
    kFeatureAnchored = 1 << 11,
  };
  using FeatureFlags = unsigned;
  virtual FeatureFlags CollectFeatureFlags() const { NOTREACHED(); }

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

  KleeneValue Evaluate(ConditionalLeafExpressionHandler&) const override;
  void CollectExpressions(HeapVector<MediaQueryExp>&) const override;
  FeatureFlags CollectFeatureFlags() const override;

 protected:
  Member<const ConditionalExpNode> operand_;
};

class CORE_EXPORT ConditionalExpNodeCompound : public ConditionalExpNode {
 public:
  ConditionalExpNodeCompound(const ConditionalExpNode* left,
                             const ConditionalExpNode* right)
      : left_(left), right_(right) {}

  void Trace(Visitor*) const override;

  void CollectExpressions(HeapVector<MediaQueryExp>&) const override;
  FeatureFlags CollectFeatureFlags() const override;

 protected:
  Member<const ConditionalExpNode> left_;
  Member<const ConditionalExpNode> right_;
};

class CORE_EXPORT ConditionalExpNodeAnd : public ConditionalExpNodeCompound {
 public:
  ConditionalExpNodeAnd(const ConditionalExpNode* left,
                        const ConditionalExpNode* right)
      : ConditionalExpNodeCompound(left, right) {}

  KleeneValue Evaluate(ConditionalLeafExpressionHandler&) const override;
  void SerializeTo(StringBuilder&) const override;
};

class CORE_EXPORT ConditionalExpNodeOr : public ConditionalExpNodeCompound {
 public:
  ConditionalExpNodeOr(const ConditionalExpNode* left,
                       const ConditionalExpNode* right)
      : ConditionalExpNodeCompound(left, right) {}

  KleeneValue Evaluate(ConditionalLeafExpressionHandler&) const override;
  void SerializeTo(StringBuilder&) const override;
};

class CORE_EXPORT ConditionalExpNodeNot : public ConditionalExpNodeUnary {
 public:
  explicit ConditionalExpNodeNot(const ConditionalExpNode* operand)
      : ConditionalExpNodeUnary(operand) {}

  KleeneValue Evaluate(ConditionalLeafExpressionHandler&) const override;
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

  void SerializeTo(StringBuilder&) const override;
  FeatureFlags CollectFeatureFlags() const override;

 private:
  AtomicString name_;
};

class CORE_EXPORT ConditionalExpNodeUnknown : public ConditionalExpNode {
 public:
  explicit ConditionalExpNodeUnknown(const String& string) : string_(string) {}

  KleeneValue Evaluate(ConditionalLeafExpressionHandler&) const override;
  void SerializeTo(StringBuilder&) const override;
  void CollectExpressions(HeapVector<MediaQueryExp>&) const override;
  FeatureFlags CollectFeatureFlags() const override;

 private:
  String string_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONDITIONAL_EXP_NODE_H_
