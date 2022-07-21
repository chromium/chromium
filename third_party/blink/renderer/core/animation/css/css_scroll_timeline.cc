// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"

#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_id_selector_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

namespace {

bool IsIdentifier(const CSSValue* value, CSSValueID value_id) {
  if (const auto* ident = DynamicTo<CSSIdentifierValue>(value))
    return ident->GetValueID() == value_id;
  return false;
}

bool IsAuto(const CSSValue* value) {
  return IsIdentifier(value, CSSValueID::kAuto);
}

bool IsNone(const CSSValue* value) {
  return IsIdentifier(value, CSSValueID::kNone);
}

const cssvalue::CSSIdSelectorValue* GetIdSelectorValue(const CSSValue* value) {
  if (const auto* selector = DynamicTo<CSSFunctionValue>(value)) {
    if (selector->FunctionType() != CSSValueID::kSelector)
      return nullptr;
    DCHECK_EQ(selector->length(), 1u);
    return DynamicTo<cssvalue::CSSIdSelectorValue>(selector->Item(0));
  }
  return nullptr;
}

absl::optional<Element*> ComputeScrollSource(Document& document,
                                             const CSSValue* value) {
  if (const auto* id = GetIdSelectorValue(value))
    return document.getElementById(id->Id());
  if (IsNone(value))
    return nullptr;
  DCHECK(!value || IsAuto(value));
  // TODO(crbug.com/1189101): Respond when the scrolling element changes.
  return absl::nullopt;
}

ScrollTimeline::ScrollDirection ComputeScrollDirection(const CSSValue* value) {
  CSSValueID value_id = CSSValueID::kAuto;

  if (const auto* identifier = DynamicTo<CSSIdentifierValue>(value))
    value_id = identifier->GetValueID();

  switch (value_id) {
    case CSSValueID::kInline:
      return ScrollTimeline::ScrollDirection::kInline;
    case CSSValueID::kHorizontal:
      return ScrollTimeline::ScrollDirection::kHorizontal;
    case CSSValueID::kVertical:
      return ScrollTimeline::ScrollDirection::kVertical;
    case CSSValueID::kAuto:
    case CSSValueID::kBlock:
    default:
      return ScrollTimeline::ScrollDirection::kBlock;
  }
}

class ElementReferenceObserver : public IdTargetObserver {
 public:
  ElementReferenceObserver(Document* document,
                           const AtomicString& id,
                           CSSScrollTimeline* timeline)
      : IdTargetObserver(document->GetIdTargetObserverRegistry(), id),
        document_(document),
        timeline_(timeline) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(timeline_);
    visitor->Trace(document_);
    IdTargetObserver::Trace(visitor);
  }

 private:
  void IdTargetChanged() override {
    if (timeline_)
      document_->GetStyleEngine().ScrollTimelineInvalidated(*timeline_);
  }
  Member<Document> document_;
  WeakMember<CSSScrollTimeline> timeline_;
};

HeapVector<Member<IdTargetObserver>> CreateElementReferenceObservers(
    Document* document,
    StyleRuleScrollTimeline* rule,
    CSSScrollTimeline* timeline) {
  HeapVector<Member<IdTargetObserver>> observers;

  if (const auto* id = GetIdSelectorValue(rule->GetSource())) {
    observers.push_back(MakeGarbageCollected<ElementReferenceObserver>(
        document, id->Id(), timeline));
  }

  return observers;
}

}  // anonymous namespace

CSSScrollTimeline::Options::Options(Document& document,
                                    StyleRuleScrollTimeline& rule)
    : source_(ComputeScrollSource(document, rule.GetSource())),
      direction_(ComputeScrollDirection(rule.GetOrientation())),
      rule_(&rule) {}

// TODO(crbug.com/1329159): Support nearest scroll ancestor.
CSSScrollTimeline::CSSScrollTimeline(Document* document, Options&& options)
    : ScrollTimeline(
          document,
          ReferenceType::kSource,
          options.source_.value_or(document->ScrollingElementNoLayout()),
          options.direction_),
      rule_(options.rule_) {
  DCHECK(rule_);
  SnapshotState();
}

bool CSSScrollTimeline::Matches(const Options& options) const {
  // TODO(crbug.com/1060384): Support ReferenceType::kNearestAncestor.
  return HasExplicitSource() && (SourceInternal() == options.source_) &&
         (GetOrientation() == options.direction_) && (rule_ == options.rule_);
}

void CSSScrollTimeline::AnimationAttached(Animation* animation) {
  if (!HasAnimations())
    SetObservers(CreateElementReferenceObservers(GetDocument(), rule_, this));
  ScrollTimeline::AnimationAttached(animation);
}

void CSSScrollTimeline::AnimationDetached(Animation* animation) {
  ScrollTimeline::AnimationDetached(animation);
  if (!HasAnimations())
    SetObservers(HeapVector<Member<IdTargetObserver>>());
}

void CSSScrollTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(rule_);
  visitor->Trace(observers_);
  ScrollTimeline::Trace(visitor);
}

void CSSScrollTimeline::SetObservers(
    HeapVector<Member<IdTargetObserver>> observers) {
  for (IdTargetObserver* observer : observers_)
    observer->Unregister();
  observers_ = std::move(observers);
}

}  // namespace blink
