// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IF_CONDITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IF_CONDITION_H_

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

class CORE_EXPORT IfTestMedia : public ConditionalExpNode {
 public:
  explicit IfTestMedia(const ConditionalExpNode* exp_node) {
    HeapVector<Member<const MediaQuery>> queries;
    queries.push_back(MakeGarbageCollected<MediaQuery>(
        MediaQuery::RestrictorType::kNone, media_type_names::kAll, exp_node));
    media_test_ = MakeGarbageCollected<MediaQuerySet>(std::move(queries));
  }
  void Trace(Visitor*) const override;

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;

 private:
  Member<const MediaQuerySet> media_test_;
};

class CORE_EXPORT IfTestSupports : public ConditionalExpNode {
 public:
  explicit IfTestSupports(bool result) : result_(result) {}
  void Trace(Visitor*) const override;

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;

 private:
  bool result_;
};

class CORE_EXPORT IfConditionElse : public ConditionalExpNode {
 public:
  explicit IfConditionElse() = default;
  void Trace(Visitor*) const override;

  KleeneValue Evaluate(ConditionalExpNodeVisitor&) const override;
  void SerializeTo(StringBuilder&) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IF_CONDITION_H_
