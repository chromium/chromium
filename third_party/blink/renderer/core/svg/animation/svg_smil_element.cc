/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/svg_set_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Compute the next time an interval with a certain (non-zero) simple duration
// will repeat, relative to a certain presentation time.
SMILTime ComputeNextRepeatTime(SMILTime interval_begin,
                               SMILTime simple_duration,
                               SMILTime presentation_time) {
  DCHECK(simple_duration);
  DCHECK_LE(interval_begin, presentation_time);
  SMILTime time_in_interval = presentation_time - interval_begin;
  SMILTime time_until_next_repeat =
      simple_duration - (time_in_interval % simple_duration);
  return presentation_time + time_until_next_repeat;
}

}  // namespace

void SMILInstanceTimeList::Append(SMILTime time, SMILTimeOrigin origin) {
  instance_times_.push_back(SMILTimeWithOrigin(time, origin));
  time_origins_.Put(origin);
}

void SMILInstanceTimeList::InsertSortedAndUnique(SMILTime time,
                                                 SMILTimeOrigin origin) {
  SMILTimeWithOrigin time_with_origin(time, origin);
  auto position = std::lower_bound(instance_times_.begin(),
                                   instance_times_.end(), time_with_origin);
  // Don't add it if we already have one of those.
  for (auto it = position; it != instance_times_.end(); ++it) {
    if (position->Time() != time)
      break;
    // If they share both time and origin, we don't need to add it,
    // we just need to react.
    if (position->Origin() == origin)
      return;
  }
  instance_times_.insert(
      static_cast<wtf_size_t>(position - instance_times_.begin()),
      time_with_origin);
  time_origins_.Put(origin);
}

void SMILInstanceTimeList::RemoveWithOrigin(SMILTimeOrigin origin) {
  if (!time_origins_.Has(origin)) {
    return;
  }
  auto tail = std::remove_if(instance_times_.begin(), instance_times_.end(),
                             [origin](const SMILTimeWithOrigin& instance_time) {
                               return instance_time.Origin() == origin;
                             });
  instance_times_.Shrink(
      static_cast<wtf_size_t>(tail - instance_times_.begin()));
  time_origins_.Remove(origin);
}

void SMILInstanceTimeList::Sort() {
  std::sort(instance_times_.begin(), instance_times_.end());
}

SMILTime SMILInstanceTimeList::NextAfter(SMILTime time) const {
  // Find the value in |list| that is strictly greater than |time|.
  auto next_item = std::lower_bound(
      instance_times_.begin(), instance_times_.end(), time,
      [](const SMILTimeWithOrigin& instance_time, const SMILTime& time) {
        return instance_time.Time() <= time;
      });
  if (next_item == instance_times_.end())
    return SMILTime::Unresolved();
  return next_item->Time();
}

// This is used for duration type time values that can't be negative.
static constexpr SMILTime kInvalidCachedTime = SMILTime::Earliest();

class ConditionEventListener final : public NativeEventListener {
 public:
  ConditionEventListener(SVGSMILElement* animation,
                         SVGSMILElement::Condition* condition)
      : animation_(animation), condition_(condition) {}

  void DisconnectAnimation() { animation_ = nullptr; }

  void Invoke(ExecutionContext*, Event*) override {
    if (!animation_)
      return;
    animation_->AddInstanceTimeAndUpdate(
        condition_->GetBeginOrEnd(),
        animation_->Elapsed() + condition_->Offset(), SMILTimeOrigin::kEvent);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(animation_);
    visitor->Trace(condition_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<SVGSMILElement> animation_;
  Member<SVGSMILElement::Condition> condition_;
};

SVGSMILElement::Condition::Condition(Type type,
                                     BeginOrEnd begin_or_end,
                                     const AtomicString& base_id,
                                     const AtomicString& name,
                                     SMILTime offset,
                                     unsigned repeat)
    : type_(type),
      begin_or_end_(begin_or_end),
      base_id_(base_id),
      name_(name),
      offset_(offset),
      repeat_(repeat) {}

SVGSMILElement::Condition::~Condition() = default;

void SVGSMILElement::Condition::Trace(Visitor* visitor) const {
  visitor->Trace(base_element_);
  visitor->Trace(base_id_observer_);
  visitor->Trace(event_listener_);
}

void SVGSMILElement::Condition::ConnectSyncBase(SVGSMILElement& timed_element) {
  DCHECK(!base_id_.empty());
  DCHECK_EQ(type_, kSyncBase);
  DCHECK(!base_element_);
  auto* svg_smil_element =
      DynamicTo<SVGSMILElement>(SVGURIReference::ObserveTarget(
          base_id_observer_, timed_element.GetTreeScope(), base_id_,
          WTF::BindRepeating(&SVGSMILElement::BuildPendingResource,
                             WrapWeakPersistent(&timed_element))));
  if (!svg_smil_element)
    return;
  base_element_ = svg_smil_element;
  svg_smil_element->AddSyncBaseDependent(timed_element);
}

void SVGSMILElement::Condition::DisconnectSyncBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kSyncBase);
  SVGURIReference::UnobserveTarget(base_id_observer_);
  if (!base_element_)
    return;
  To<SVGSMILElement>(*base_element_).RemoveSyncBaseDependent(timed_element);
  base_element_ = nullptr;
}

void SVGSMILElement::Condition::ConnectEventBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kEventBase);
  DCHECK(!base_element_);
  DCHECK(!event_listener_);
  Element* target;
  if (base_id_.empty()) {
    target = timed_element.targetElement();
  } else {
    target = SVGURIReference::ObserveTarget(
        base_id_observer_, timed_element.GetTreeScope(), base_id_,
        WTF::BindRepeating(&SVGSMILElement::BuildPendingResource,
                           WrapWeakPersistent(&timed_element)));
  }
  if (!target)
    return;
  event_listener_ =
      MakeGarbageCollected<ConditionEventListener>(&timed_element, this);
  base_element_ = target;
  base_element_->addEventListener(name_, event_listener_, false);
}

void SVGSMILElement::Condition::DisconnectEventBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kEventBase);
  SVGURIReference::UnobserveTarget(base_id_observer_);
  if (!event_listener_)
    return;
  base_element_->removeEventListener(name_, event_listener_, false);
  base_element_ = nullptr;
  event_listener_->DisconnectAnimation();
  event_listener_ = nullptr;
}

SVGSMILElement::SVGSMILElement(const QualifiedName& tag_name, Document& doc)
    : SVGElement(tag_name, doc),
      SVGTests(this),
      target_element_(nullptr),
      conditions_connected_(false),
      has_end_event_conditions_(false),
      is_waiting_for_first_interval_(true),
      is_scheduled_(false),
      interval_(SMILInterval::Unresolved()),
      previous_interval_(SMILInterval::Unresolved()),
      active_state_(kInactive),
      restart_(kRestartAlways),
      fill_(kFillRemove),
      last_progress_{0.0f, 0},
      document_order_index_(0),
      queue_handle_(kNotFound),
      cached_dur_(kInvalidCachedTime),
      cached_repeat_dur_(kInvalidCachedTime),
      cached_repeat_count_(SMILRepeatCount::Invalid()),
      cached_min_(kInvalidCachedTime),
      cached_max_(kInvalidCachedTime),
      interval_has_changed_(false),
      instance_lists_have_changed_(false),
      interval_needs_revalidation_(false),
      is_notifying_dependents_(false) {}

SVGSMILElement::~SVGSMILElement() = default;

void SVGSMILElement::ClearResourceAndEventBaseReferences() {
  SVGURIReference::UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGSMILElement::BuildPendingResource() {
  ClearResourceAndEventBaseReferences();
  DisconnectConditions();

  if (!isConnected()) {
    // Reset the target element if we are no longer in the document.
    SetTargetElement(nullptr);
    return;
  }

  const AtomicString& href = SVGURIReference::LegacyHrefString(*this);
  Element* target;
  if (href.empty()) {
    target = parentElement();
  } else {
    target = SVGURIReference::ObserveTarget(target_id_observer_, *this, href);
  }
  auto* svg_target = DynamicTo<SVGElement>(target);

  if (svg_target && !svg_target->isConnected())
    svg_target = nullptr;

  SetTargetElement(svg_target);

  if (svg_target) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(svg_target);
  }
  ConnectConditions();
}

void SVGSMILElement::Reset() {
  active_state_ = kInactive;
  is_waiting_for_first_interval_ = true;
  interval_ = SMILInterval::Unresolved();
  previous_interval_ = SMILInterval::Unresolved();
  last_progress_ = {0.0f, 0};
}

Node::InsertionNotificationRequest SVGSMILElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGElement::InsertedInto(root_parent);

  if (!root_parent.isConnected())
    return kInsertionDone;

  UseCounter::Count(GetDocument(), WebFeature::kSVGSMILElementInDocument);
  if (GetDocument().IsLoadCompleted()) {
    UseCounter::Count(&GetDocument(),
                      WebFeature::kSVGSMILElementInsertedAfterLoad);
  }

  SVGSVGElement* owner = ownerSVGElement();
  if (!owner)
    return kInsertionDone;

  time_container_ = owner->TimeContainer();
  DCHECK(time_container_);
  time_container_->SetDocumentOrderIndexesDirty();

  // "If no attribute is present, the default begin value (an offset-value of 0)
  // must be evaluated."
  if (!FastHasAttribute(svg_names::kBeginAttr) && begin_times_.IsEmpty())
    begin_times_.Append(SMILTime(), SMILTimeOrigin::kAttribute);

  BuildPendingResource();
  return kInsertionDone;
}

void SVGSMILElement::RemovedFrom(ContainerNode& root_parent) {
  if (root_parent.isConnected()) {
    ClearResourceAndEventBaseReferences();
    ClearConditions();
    SetTargetElement(nullptr);
    time_container_ = nullptr;
  }

  SVGElement::RemovedFrom(root_parent);
}

SMILTime SVGSMILElement::ParseOffsetValue(const String& data) {
  bool ok;
  double result = 0;
  const String parse = data.StripWhiteSpace();
  if (parse.EndsWith('h')) {
    result = parse.Left(parse.length() - 1).ToDouble(&ok) *
             base::Time::kSecondsPerHour;
  } else if (parse.EndsWith("min")) {
    result = parse.Left(parse.length() - 3).ToDouble(&ok) *
             base::Time::kSecondsPerMinute;
  } else if (parse.EndsWith("ms")) {
    result = parse.Left(parse.length() - 2).ToDouble(&ok) /
             base::Time::kMillisecondsPerSecond;
  } else if (parse.EndsWith('s')) {
    result = parse.Left(parse.length() - 1).ToDouble(&ok);
  } else {
    result = parse.ToDouble(&ok);
  }
  return ok ? SMILTime::FromSecondsD(result) : SMILTime::Unresolved();
}

SMILTime SVGSMILElement::ParseClockValue(const String& data) {
  if (data.IsNull())
    return SMILTime::Unresolved();

  String parse = data.StripWhiteSpace();

  DEFINE_STATIC_LOCAL(const AtomicString, indefinite_value, ("indefinite"));
  if (parse == indefinite_value)
    return SMILTime::Indefinite();

  double result = 0;
  bool ok;
  wtf_size_t double_point_one = parse.find(':');
  wtf_size_t double_point_two = parse.find(':', double_point_one + 1);
  if (double_point_one == 2 && double_point_two == 5 && parse.length() >= 8) {
    result += parse.Substring(0, 2).ToUIntStrict(&ok) * 60 * 60;
    if (!ok)
      return SMILTime::Unresolved();
    result += parse.Substring(3, 2).ToUIntStrict(&ok) * 60;
    if (!ok)
      return SMILTime::Unresolved();
    result += parse.Substring(6).ToDouble(&ok);
  } else if (double_point_one == 2 && double_point_two == kNotFound &&
             parse.length() >= 5) {
    result += parse.Substring(0, 2).ToUIntStrict(&ok) * 60;
    if (!ok)
      return SMILTime::Unresolved();
    result += parse.Substring(3).ToDouble(&ok);
  } else {
    return ParseOffsetValue(parse);
  }

  if (!ok)
    return SMILTime::Unresolved();
  return SMILTime::FromSecondsD(result);
}

bool SVGSMILElement::ParseCondition(const String& value,
                                    BeginOrEnd begin_or_end) {
  String parse_string = value.StripWhiteSpace();

  bool is_negated = false;
  bool ok;
  wtf_size_t pos = parse_string.find('+');
  if (pos == kNotFound) {
    pos = parse_string.find('-');
    is_negated = pos != kNotFound;
  }
  String condition_string;
  SMILTime offset;
  if (pos == kNotFound) {
    condition_string = parse_string;
  } else {
    condition_string = parse_string.Left(pos).StripWhiteSpace();
    String offset_string = parse_string.Substring(pos + 1).StripWhiteSpace();
    offset = ParseOffsetValue(offset_string);
    if (offset.IsUnresolved())
      return false;
    if (is_negated)
      offset = -offset;
  }
  if (condition_string.empty())
    return false;
  pos = condition_string.find('.');

  String base_id;
  String name_string;
  if (pos == kNotFound) {
    name_string = condition_string;
  } else {
    base_id = condition_string.Left(pos);
    name_string = condition_string.Substring(pos + 1);
  }
  if (name_string.empty())
    return false;

  Condition::Type type;
  int repeat = -1;
  if (name_string.StartsWith("repeat(") && name_string.EndsWith(')')) {
    repeat =
        name_string.Substring(7, name_string.length() - 8).ToUIntStrict(&ok);
    if (!ok)
      return false;
    name_string = "repeat";
    type = Condition::kSyncBase;
  } else if (name_string == "begin" || name_string == "end") {
    if (base_id.empty())
      return false;
    UseCounter::Count(&GetDocument(),
                      WebFeature::kSVGSMILBeginOrEndSyncbaseValue);
    type = Condition::kSyncBase;
  } else if (name_string.StartsWith("accesskey(")) {
    // FIXME: accesskey() support.
    type = Condition::kAccessKey;
  } else {
    UseCounter::Count(&GetDocument(), WebFeature::kSVGSMILBeginOrEndEventValue);
    type = Condition::kEventBase;
  }

  conditions_.push_back(MakeGarbageCollected<Condition>(
      type, begin_or_end, AtomicString(base_id), AtomicString(name_string),
      offset, repeat));

  if (type == Condition::kEventBase && begin_or_end == kEnd)
    has_end_event_conditions_ = true;

  return true;
}

void SVGSMILElement::ParseBeginOrEnd(const String& parse_string,
                                     BeginOrEnd begin_or_end) {
  auto& time_list = begin_or_end == kBegin ? begin_times_ : end_times_;
  if (begin_or_end == kEnd)
    has_end_event_conditions_ = false;

  // Remove any previously added offset-values.
  // TODO(fs): Ought to remove instance times originating from sync-bases,
  // events etc. as well if those conditions are no longer in the attribute.
  time_list.RemoveWithOrigin(SMILTimeOrigin::kAttribute);

  Vector<String> split_string;
  parse_string.Split(';', split_string);
  for (const auto& item : split_string) {
    SMILTime value = ParseClockValue(item);
    if (value.IsUnresolved())
      ParseCondition(item, begin_or_end);
    else
      time_list.Append(value, SMILTimeOrigin::kAttribute);
  }
  // "If no attribute is present, the default begin value (an offset-value of 0)
  // must be evaluated."
  if (begin_or_end == kBegin && parse_string.IsNull())
    begin_times_.Append(SMILTime(), SMILTimeOrigin::kAttribute);

  time_list.Sort();
}

void SVGSMILElement::ParseAttribute(const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == svg_names::kBeginAttr) {
    if (!conditions_.empty()) {
      ClearConditions();
      ParseBeginOrEnd(FastGetAttribute(svg_names::kEndAttr), kEnd);
    }
    ParseBeginOrEnd(value.GetString(), kBegin);
    if (isConnected()) {
      ConnectConditions();
      instance_lists_have_changed_ = true;
      InstanceListChanged();
    }
  } else if (name == svg_names::kEndAttr) {
    if (!conditions_.empty()) {
      ClearConditions();
      ParseBeginOrEnd(FastGetAttribute(svg_names::kBeginAttr), kBegin);
    }
    ParseBeginOrEnd(value.GetString(), kEnd);
    if (isConnected()) {
      ConnectConditions();
      instance_lists_have_changed_ = true;
      InstanceListChanged();
    }
  } else if (name == svg_names::kOnbeginAttr) {
    SetAttributeEventListener(event_type_names::kBeginEvent,
                              JSEventHandlerForContentAttribute::Create(
                                  GetExecutionContext(), name, value));
  } else if (name == svg_names::kOnendAttr) {
    SetAttributeEventListener(event_type_names::kEndEvent,
                              JSEventHandlerForContentAttribute::Create(
                                  GetExecutionContext(), name, value));
  } else if (name == svg_names::kOnrepeatAttr) {
    SetAttributeEventListener(event_type_names::kRepeatEvent,
                              JSEventHandlerForContentAttribute::Create(
                                  GetExecutionContext(), name, value));
  } else if (name == svg_names::kRestartAttr) {
    if (value == "never")
      restart_ = kRestartNever;
    else if (value == "whenNotActive")
      restart_ = kRestartWhenNotActive;
    else
      restart_ = kRestartAlways;
  } else if (name == svg_names::kFillAttr) {
    fill_ = value == "freeze" ? kFillFreeze : kFillRemove;
  } else if (name == svg_names::kDurAttr) {
    cached_dur_ = kInvalidCachedTime;
    IntervalStateChanged();
  } else if (name == svg_names::kRepeatDurAttr) {
    cached_repeat_dur_ = kInvalidCachedTime;
    IntervalStateChanged();
  } else if (name == svg_names::kRepeatCountAttr) {
    cached_repeat_count_ = SMILRepeatCount::Invalid();
    IntervalStateChanged();
  } else if (name == svg_names::kMinAttr) {
    cached_min_ = kInvalidCachedTime;
    IntervalStateChanged();
  } else if (name == svg_names::kMaxAttr) {
    cached_max_ = kInvalidCachedTime;
    IntervalStateChanged();
  } else if (SVGURIReference::IsKnownAttribute(name)) {
    // TODO(fs): Could be smarter here when 'href' is specified and 'xlink:href'
    // is changed.
    BuildPendingResource();
  } else {
    SVGElement::ParseAttribute(params);
  }
}

bool SVGSMILElement::IsPresentationAttribute(
    const QualifiedName& attr_name) const {
  // Don't map 'fill' to the 'fill' property for animation elements.
  if (attr_name == svg_names::kFillAttr)
    return false;
  return SVGElement::IsPresentationAttribute(attr_name);
}

void SVGSMILElement::CollectStyleForPresentationAttribute(
    const QualifiedName& attr_name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (attr_name == svg_names::kFillAttr)
    return;
  SVGElement::CollectStyleForPresentationAttribute(attr_name, value, style);
}

SVGAnimatedPropertyBase* SVGSMILElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (SVGAnimatedPropertyBase* property =
          SVGTests::PropertyFromAttribute(attribute_name)) {
    return property;
  }
  return SVGElement::PropertyFromAttribute(attribute_name);
}

void SVGSMILElement::ConnectConditions() {
  if (conditions_connected_)
    DisconnectConditions();
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kSyncBase)
      condition->ConnectSyncBase(*this);
    else if (condition->GetType() == Condition::kEventBase)
      condition->ConnectEventBase(*this);
  }
  conditions_connected_ = true;
}

void SVGSMILElement::DisconnectConditions() {
  if (!conditions_connected_)
    return;
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kSyncBase)
      condition->DisconnectSyncBase(*this);
    else if (condition->GetType() == Condition::kEventBase)
      condition->DisconnectEventBase(*this);
  }
  conditions_connected_ = false;
}

void SVGSMILElement::ClearConditions() {
  DisconnectConditions();
  conditions_.clear();
}

void SVGSMILElement::SetTargetElement(SVGElement* target) {
  if (target == target_element_)
    return;
  WillChangeAnimationTarget();
  target_element_ = target;
  DidChangeAnimationTarget();
}

SMILTime SVGSMILElement::Elapsed() const {
  return time_container_ ? time_container_->Elapsed() : SMILTime();
}

SMILTime SVGSMILElement::BeginTimeForPrioritization(
    SMILTime presentation_time) const {
  if (GetActiveState() == kFrozen) {
    if (interval_.BeginsAfter(presentation_time))
      return previous_interval_.begin;
  }
  return interval_.begin;
}

bool SVGSMILElement::IsHigherPriorityThan(const SVGSMILElement* other,
                                          SMILTime presentation_time) const {
  // FIXME: This should also consider possible timing relations between the
  // elements.
  SMILTime this_begin = BeginTimeForPrioritization(presentation_time);
  SMILTime other_begin = other->BeginTimeForPrioritization(presentation_time);
  if (this_begin == other_begin)
    return DocumentOrderIndex() > other->DocumentOrderIndex();
  return this_begin > other_begin;
}

SMILTime SVGSMILElement::Dur() const {
  if (cached_dur_ != kInvalidCachedTime)
    return cached_dur_;
  const AtomicString& value = FastGetAttribute(svg_names::kDurAttr);
  SMILTime clock_value = ParseClockValue(value);
  return cached_dur_ =
             clock_value <= SMILTime() ? SMILTime::Unresolved() : clock_value;
}

SMILTime SVGSMILElement::RepeatDur() const {
  if (cached_repeat_dur_ != kInvalidCachedTime)
    return cached_repeat_dur_;
  const AtomicString& value = FastGetAttribute(svg_names::kRepeatDurAttr);
  SMILTime clock_value = ParseClockValue(value);
  cached_repeat_dur_ =
      clock_value <= SMILTime() ? SMILTime::Unresolved() : clock_value;
  return cached_repeat_dur_;
}

static SMILRepeatCount ParseRepeatCount(const AtomicString& value) {
  if (value.IsNull())
    return SMILRepeatCount::Unspecified();
  if (value == "indefinite")
    return SMILRepeatCount::Indefinite();
  bool ok;
  double result = value.ToDouble(&ok);
  if (ok && result > 0 && std::isfinite(result))
    return SMILRepeatCount::Numeric(result);
  return SMILRepeatCount::Unspecified();
}

SMILRepeatCount SVGSMILElement::RepeatCount() const {
  if (!cached_repeat_count_.IsValid()) {
    cached_repeat_count_ =
        ParseRepeatCount(FastGetAttribute(svg_names::kRepeatCountAttr));
  }
  DCHECK(cached_repeat_count_.IsValid());
  return cached_repeat_count_;
}

SMILTime SVGSMILElement::MaxValue() const {
  if (cached_max_ != kInvalidCachedTime)
    return cached_max_;
  const AtomicString& value = FastGetAttribute(svg_names::kMaxAttr);
  SMILTime result = ParseClockValue(value);
  return cached_max_ = (result.IsUnresolved() || result <= SMILTime())
                           ? SMILTime::Indefinite()
                           : result;
}

SMILTime SVGSMILElement::MinValue() const {
  if (cached_min_ != kInvalidCachedTime)
    return cached_min_;
  const AtomicString& value = FastGetAttribute(svg_names::kMinAttr);
  SMILTime result = ParseClockValue(value);
  return cached_min_ = (result.IsUnresolved() || result < SMILTime())
                           ? SMILTime()
                           : result;
}

SMILTime SVGSMILElement::SimpleDuration() const {
  return std::min(Dur(), SMILTime::Indefinite());
}

void SVGSMILElement::AddInstanceTime(BeginOrEnd begin_or_end,
                                     SMILTime time,
                                     SMILTimeOrigin origin) {
  auto& list = begin_or_end == kBegin ? begin_times_ : end_times_;
  list.InsertSortedAndUnique(time, origin);
  instance_lists_have_changed_ = true;
}

void SVGSMILElement::AddInstanceTimeAndUpdate(BeginOrEnd begin_or_end,
                                              SMILTime time,
                                              SMILTimeOrigin origin) {
  // Ignore new instance times for 'end' if the element is not active
  // and the origin is script.
  if (begin_or_end == kEnd && GetActiveState() == kInactive &&
      origin == SMILTimeOrigin::kScript)
    return;
  AddInstanceTime(begin_or_end, time, origin);
  InstanceListChanged();
}

SMILTime SVGSMILElement::RepeatingDuration() const {
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILRepeatCount repeat_count = RepeatCount();
  SMILTime repeat_dur = RepeatDur();
  SMILTime simple_duration = SimpleDuration();
  if (!simple_duration ||
      (repeat_dur.IsUnresolved() && repeat_count.IsUnspecified()))
    return simple_duration;
  repeat_dur = std::min(repeat_dur, SMILTime::Indefinite());
  SMILTime repeat_count_duration = simple_duration.Repeat(repeat_count);
  if (!repeat_count_duration.IsUnresolved())
    return std::min(repeat_dur, repeat_count_duration);
  return repeat_dur;
}

SMILTime SVGSMILElement::ResolveActiveEnd(SMILTime resolved_begin) const {
  SMILTime resolved_end = SMILTime::Indefinite();
  if (!end_times_.IsEmpty()) {
    SMILTime next_end = end_times_.NextAfter(resolved_begin);
    if (next_end.IsUnresolved()) {
      // If we have no pending end conditions, don't generate a new interval.
      if (!has_end_event_conditions_)
        return SMILTime::Unresolved();
    } else {
      resolved_end = next_end;
    }
  }
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILTime preliminary_active_duration;
  if (!resolved_end.IsUnresolved() && Dur().IsUnresolved() &&
      RepeatDur().IsUnresolved() && RepeatCount().IsUnspecified())
    preliminary_active_duration = resolved_end - resolved_begin;
  else if (!resolved_end.IsFinite())
    preliminary_active_duration = RepeatingDuration();
  else
    preliminary_active_duration =
        std::min(RepeatingDuration(), resolved_end - resolved_begin);

  SMILTime min_value = MinValue();
  SMILTime max_value = MaxValue();
  if (min_value > max_value) {
    // Ignore both.
    // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#MinMax
    min_value = SMILTime();
    max_value = SMILTime::Indefinite();
  }
  return resolved_begin +
         std::min(max_value, std::max(min_value, preliminary_active_duration));
}

SMILInterval SVGSMILElement::ResolveInterval(SMILTime begin_after,
                                             SMILTime end_after) {
  const bool first = is_waiting_for_first_interval_;
  // Simplified version of the pseudocode in
  // http://www.w3.org/TR/SMIL3/smil-timing.html#q90.
  const size_t kMaxIterations = std::max(begin_times_.size() * 4, 1000000u);
  size_t current_iteration = 0;
  for (auto search_start = begin_times_.begin();
       search_start != begin_times_.end(); ++search_start) {
    // Find the (next) instance time in the 'begin' list that is greater or
    // equal to |begin_after|.
    auto begin_item = std::lower_bound(
        search_start, begin_times_.end(), begin_after,
        [](const SMILTimeWithOrigin& instance_time, const SMILTime& time) {
          return instance_time.Time() < time;
        });
    // If there are no more 'begin' instance times, or we encountered the
    // special value "indefinite" (which doesn't yield an instance time in the
    // 'begin' list), we're done.
    if (begin_item == begin_times_.end() || begin_item->Time().IsIndefinite())
      break;
    SMILTime temp_end = ResolveActiveEnd(begin_item->Time());
    if (temp_end.IsUnresolved())
      break;
    SMILInterval interval(begin_item->Time(), temp_end);
    // Don't allow the interval to end in the past.
    if (temp_end > end_after)
      return interval;
    // The resolved interval was in the past. If it's the first interval being
    // resolved, then update interval state since it could be active (frozen).
    // Skip the interval if it ends before the time container starts
    // (presentation time is 0).
    if (first && temp_end > SMILTime()) {
      interval_ = interval;
      is_waiting_for_first_interval_ = false;
    }
    // Ensure forward progress by only considering the part of the 'begin' list
    // after |begin_item| for the next iteration.
    search_start = begin_item;
    begin_after = temp_end;
    // Debugging signal for crbug.com/1021630.
    CHECK_LT(current_iteration++, kMaxIterations);
  }
  return SMILInterval::Unresolved();
}

void SVGSMILElement::SetNewInterval(const SMILInterval& interval) {
  interval_ = interval;
  NotifyDependentsOnNewInterval(interval_);
}

void SVGSMILElement::SetNewIntervalEnd(SMILTime new_end) {
  interval_.end = new_end;
  NotifyDependentsOnNewInterval(interval_);
}

SMILTime SVGSMILElement::ComputeNextIntervalTime(
    SMILTime presentation_time,
    IncludeRepeats include_repeats) const {
  SMILTime next_interval_time = SMILTime::Unresolved();
  if (interval_.BeginsAfter(presentation_time)) {
    next_interval_time = interval_.begin;
  } else if (interval_.EndsAfter(presentation_time)) {
    SMILTime simple_duration = SimpleDuration();
    if (include_repeats == kIncludeRepeats && simple_duration) {
      SMILTime next_repeat_time = ComputeNextRepeatTime(
          interval_.begin, simple_duration, presentation_time);
      DCHECK(next_repeat_time.IsFinite());
      next_interval_time = std::min(next_repeat_time, interval_.end);
    } else {
      next_interval_time = interval_.end;
    }
  }
  SMILTime next_begin = begin_times_.NextAfter(presentation_time);
  // The special value "indefinite" does not yield an instance time in the
  // begin list, so only consider finite values here.
  if (next_begin.IsFinite())
    next_interval_time = std::min(next_interval_time, next_begin);
  return next_interval_time;
}

void SVGSMILElement::InstanceListChanged() {
  DCHECK(instance_lists_have_changed_);
  SMILTime current_presentation_time =
      time_container_ ? time_container_->LatestUpdatePresentationTime()
                      : SMILTime();
  DCHECK(!current_presentation_time.IsUnresolved());
  const bool was_active = GetActiveState() == kActive;
  UpdateInterval(current_presentation_time);
  // Check active state and reschedule using the time just before the current
  // presentation time. This means that the next animation update will take
  // care of updating the active state and send events as needed.
  SMILTime previous_presentation_time =
      current_presentation_time - SMILTime::Epsilon();
  if (was_active) {
    const SMILInterval& active_interval =
        GetActiveInterval(previous_presentation_time);
    active_state_ =
        DetermineActiveState(active_interval, previous_presentation_time);
    if (GetActiveState() != kActive)
      EndedActiveInterval();
  }
  if (time_container_) {
    SMILTime next_interval_time;
    // If we switched interval and the previous interval did not end yet, we
    // need to consider it when computing the next interval time.
    if (previous_interval_.IsResolved() &&
        previous_interval_.EndsAfter(previous_presentation_time)) {
      next_interval_time = previous_interval_.end;
    } else {
      next_interval_time =
          ComputeNextIntervalTime(previous_presentation_time, kIncludeRepeats);
    }
    time_container_->Reschedule(this, next_interval_time);
  }
}

void SVGSMILElement::IntervalStateChanged() {
  if (!isConnected() || !time_container_) {
    return;
  }
  // Make the time container re-evaluate the interval.
  time_container_->Reschedule(this, SMILTime::Earliest());
  interval_needs_revalidation_ = true;
}

void SVGSMILElement::DiscardOrRevalidateCurrentInterval(
    SMILTime presentation_time) {
  if (!interval_.IsResolved())
    return;
  // If the current interval has not yet started, discard it and re-resolve.
  if (interval_.BeginsAfter(presentation_time)) {
    interval_ = SMILInterval::Unresolved();
    return;
  }

  // If we have a current interval but it has not yet ended, re-resolve the
  // end time.
  if (interval_.EndsAfter(presentation_time)) {
    SMILTime new_end = ResolveActiveEnd(interval_.begin);
    if (new_end.IsUnresolved()) {
      // No active duration, discard the current interval.
      interval_ = SMILInterval::Unresolved();
      // If we discarded the first interval, revert to waiting for the first
      // interval.
      if (!previous_interval_.IsResolved())
        is_waiting_for_first_interval_ = true;
      return;
    }
    if (new_end != interval_.end)
      SetNewIntervalEnd(new_end);
  }
}

bool SVGSMILElement::HandleIntervalRestart(SMILTime presentation_time) {
  Restart restart = GetRestart();
  if (!is_waiting_for_first_interval_ && restart == kRestartNever)
    return false;
  if (!interval_.IsResolved() || interval_.EndsBefore(presentation_time))
    return true;
  if (restart == kRestartAlways) {
    SMILTime next_begin = begin_times_.NextAfter(interval_.begin);
    if (interval_.EndsAfter(next_begin)) {
      SetNewIntervalEnd(next_begin);
      return interval_.EndsBefore(presentation_time);
    }
  }
  return false;
}

SMILTime SVGSMILElement::LastIntervalEndTime() const {
  // If we're still waiting for the first interval we lack a time reference.
  if (!is_waiting_for_first_interval_) {
    // If we have a current interval (which likely just ended or restarted) use
    // the end of that.
    if (interval_.IsResolved())
      return interval_.end;
    // If we don't have a current interval (maybe because it got discarded for
    // not having started yet) but we have a previous interval, then use the
    // end of that.
    if (previous_interval_.IsResolved())
      return previous_interval_.end;
  }
  // We have to start from the beginning.
  return SMILTime::Earliest();
}

void SVGSMILElement::UpdateInterval(SMILTime presentation_time) {
  if (instance_lists_have_changed_ || interval_needs_revalidation_) {
    instance_lists_have_changed_ = false;
    interval_needs_revalidation_ = false;
    DiscardOrRevalidateCurrentInterval(presentation_time);
  }
  if (!HandleIntervalRestart(presentation_time))
    return;
  SMILTime begin_after = LastIntervalEndTime();
  SMILInterval next_interval = ResolveInterval(begin_after, presentation_time);
  // It's the same interval that we resolved before. Do nothing.
  if (next_interval == interval_)
    return;
  interval_has_changed_ = true;
  if (interval_.IsResolved())
    previous_interval_ = interval_;
  // If there are no more intervals to resolve, we have to wait for an event to
  // occur in order to get a new instance time.
  if (!next_interval.IsResolved()) {
    interval_ = next_interval;
    return;
  }
  SetNewInterval(next_interval);
}

void SVGSMILElement::AddedToTimeContainer() {
  DCHECK(time_container_);
  SMILTime current_presentation_time =
      time_container_->LatestUpdatePresentationTime();
  UpdateInterval(current_presentation_time);
  // Check active state and reschedule using the time just before the current
  // presentation time. This means that the next animation update will take
  // care of updating the active state and send events as needed.
  SMILTime previous_presentation_time =
      current_presentation_time - SMILTime::Epsilon();
  active_state_ = DetermineActiveState(interval_, previous_presentation_time);
  time_container_->Reschedule(
      this,
      ComputeNextIntervalTime(previous_presentation_time, kIncludeRepeats));

  // If there's an active interval, then revalidate the animation value.
  if (GetActiveState() != kInactive) {
    StartedActiveInterval();
    // Dispatch a 'beginEvent' if the timeline has started and the interval is
    // active.
    if (GetActiveState() == kActive && time_container_->IsStarted()) {
      DispatchEvents(kDispatchBeginEvent);
    }
  }
}

void SVGSMILElement::RemovedFromTimeContainer() {
  DCHECK(time_container_);
  // If the element is active reset to a clear state.
  if (GetActiveState() != kInactive) {
    EndedActiveInterval();
    // Dispatch a 'endEvent' if the timeline has started and the interval is
    // (was) active.
    if (GetActiveState() == kActive && time_container_->IsStarted()) {
      DispatchEvents(kDispatchEndEvent);
    }
  }
}

const SMILInterval& SVGSMILElement::GetActiveInterval(SMILTime elapsed) const {
  // If there's no current interval, return the previous interval.
  if (!interval_.IsResolved())
    return previous_interval_;
  // If there's a previous interval and the current interval hasn't begun yet,
  // return the previous interval.
  if (previous_interval_.IsResolved() && interval_.BeginsAfter(elapsed))
    return previous_interval_;
  return interval_;
}

SVGSMILElement::ProgressState SVGSMILElement::CalculateProgressState(
    SMILTime presentation_time) const {
  const SMILTime simple_duration = SimpleDuration();
  if (simple_duration.IsIndefinite())
    return {0.0f, 0};
  if (!simple_duration)
    return {1.0f, 0};
  DCHECK(simple_duration.IsFinite());
  const SMILInterval& active_interval = GetActiveInterval(presentation_time);
  DCHECK(active_interval.IsResolved());
  const SMILTime active_time = presentation_time - active_interval.begin;
  const SMILTime repeating_duration = RepeatingDuration();
  int64_t repeat;
  SMILTime simple_time;
  if (presentation_time >= active_interval.end ||
      active_time > repeating_duration) {
    // Use the interval to compute the interval position if we've passed the
    // interval end, otherwise use the "repeating duration". This prevents a
    // stale interval (with for instance an 'indefinite' end) from yielding an
    // invalid interval position.
    SMILTime last_active_duration =
        presentation_time >= active_interval.end
            ? active_interval.end - active_interval.begin
            : repeating_duration;
    if (!last_active_duration.IsFinite())
      return {0.0f, 0};
    // If the repeat duration is a multiple of the simple duration, we should
    // use a progress value of 1.0, otherwise we should return a value that is
    // within the interval (< 1.0), so subtract the smallest representable time
    // delta in that case.
    repeat = last_active_duration.IntDiv(simple_duration);
    simple_time = last_active_duration % simple_duration;
    if (simple_time) {
      simple_time = simple_time - SMILTime::Epsilon();
    } else {
      simple_time = simple_duration;
      --repeat;
    }
  } else {
    repeat = active_time.IntDiv(simple_duration);
    simple_time = active_time % simple_duration;
  }
  return {ClampTo<float>(simple_time.InternalValueAsDouble() /
                         simple_duration.InternalValueAsDouble()),
          ClampTo<unsigned>(repeat)};
}

SMILTime SVGSMILElement::NextProgressTime(SMILTime presentation_time) const {
  if (GetActiveState() == kActive) {
    // If duration is indefinite the value does not actually change over time.
    // Same is true for <set>.
    SMILTime simple_duration = SimpleDuration();
    if (simple_duration.IsIndefinite() || IsA<SVGSetElement>(*this)) {
      SMILTime repeating_duration_end = interval_.begin + RepeatingDuration();
      // We are supposed to do freeze semantics when repeating ends, even if the
      // element is still active.
      // Take care that we get a timer callback at that point.
      if (presentation_time < repeating_duration_end &&
          interval_.EndsAfter(repeating_duration_end) &&
          repeating_duration_end.IsFinite())
        return repeating_duration_end;
      return interval_.end;
    }
    return presentation_time;
  }
  return interval_.begin >= presentation_time ? interval_.begin
                                              : SMILTime::Unresolved();
}

SVGSMILElement::ActiveState SVGSMILElement::DetermineActiveState(
    const SMILInterval& interval,
    SMILTime elapsed) const {
  if (interval.Contains(elapsed))
    return kActive;
  if (is_waiting_for_first_interval_)
    return kInactive;
  return Fill() == kFillFreeze ? kFrozen : kInactive;
}

bool SVGSMILElement::IsContributing(SMILTime elapsed) const {
  // Animation does not contribute during the active time if it is past its
  // repeating duration and has fill=remove.
  return (GetActiveState() == kActive &&
          (Fill() == kFillFreeze ||
           elapsed <= interval_.begin + RepeatingDuration())) ||
         GetActiveState() == kFrozen;
}

SVGSMILElement::EventDispatchMask SVGSMILElement::UpdateActiveState(
    SMILTime presentation_time,
    bool skip_repeat) {
  const bool was_active = GetActiveState() == kActive;
  active_state_ = DetermineActiveState(interval_, presentation_time);
  const bool is_active = GetActiveState() == kActive;
  const bool interval_restart =
      interval_has_changed_ && previous_interval_.end == interval_.begin;
  interval_has_changed_ = false;

  unsigned events_to_dispatch = kDispatchNoEvent;
  if ((was_active && !is_active) || interval_restart) {
    events_to_dispatch |= kDispatchEndEvent;
    EndedActiveInterval();
  }

  if (IsContributing(presentation_time)) {
    if ((!was_active && is_active) || interval_restart) {
      events_to_dispatch |= kDispatchBeginEvent;
      StartedActiveInterval();
    }

    if (!skip_repeat) {
      // TODO(fs): This is a bit fragile. Convert to be time-based (rather than
      // based on |last_progress_|) and thus (at least more) idempotent.
      ProgressState progress_state = CalculateProgressState(presentation_time);
      if (progress_state.repeat &&
          progress_state.repeat != last_progress_.repeat) {
        NotifyDependentsOnRepeat(progress_state.repeat, presentation_time);
        events_to_dispatch |= kDispatchRepeatEvent;
      }
      last_progress_ = progress_state;
    }
  }
  return static_cast<EventDispatchMask>(events_to_dispatch);
}

SVGSMILElement::EventDispatchMask SVGSMILElement::ComputeSeekEvents(
    const SMILInterval& starting_interval) const {
  // Is the element active at the seeked-to time?
  if (GetActiveState() != SVGSMILElement::kActive) {
    // Was the element active at the previous time?
    if (!starting_interval.IsResolved())
      return kDispatchNoEvent;
    // Dispatch an 'endEvent' for ending |starting_interval|.
    return kDispatchEndEvent;
  }
  // Was the element active at the previous time?
  if (starting_interval.IsResolved()) {
    // The same interval?
    if (interval_ == starting_interval)
      return kDispatchNoEvent;
    // Dispatch an 'endEvent' for ending |starting_interval| and a 'beginEvent'
    // for beginning the current interval.
    unsigned to_dispatch = kDispatchBeginEvent | kDispatchEndEvent;
    return static_cast<EventDispatchMask>(to_dispatch);
  }
  // Was not active at the previous time. Dispatch a 'beginEvent' for beginning
  // the current interval.
  return kDispatchBeginEvent;
}

void SVGSMILElement::DispatchEvents(EventDispatchMask events_to_dispatch) {
  // The ordering is based on the usual order in which these events should be
  // dispatched (and should match the order the flags are set in
  // UpdateActiveState().
  if (events_to_dispatch & kDispatchEndEvent) {
    EnqueueEvent(*Event::Create(event_type_names::kEndEvent),
                 TaskType::kDOMManipulation);
  }
  if (events_to_dispatch & kDispatchBeginEvent) {
    EnqueueEvent(*Event::Create(event_type_names::kBeginEvent),
                 TaskType::kDOMManipulation);
  }
  if (events_to_dispatch & kDispatchRepeatEvent) {
    EnqueueEvent(*Event::Create(event_type_names::kRepeatEvent),
                 TaskType::kDOMManipulation);
    EnqueueEvent(*Event::Create(AtomicString("repeatn")),
                 TaskType::kDOMManipulation);
  }
}

void SVGSMILElement::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  SVGElement::AddedEventListener(event_type, registered_listener);
  if (event_type == "repeatn") {
    UseCounter::Count(GetDocument(),
                      WebFeature::kSMILElementHasRepeatNEventListener);
  }
}

void SVGSMILElement::UpdateProgressState(SMILTime presentation_time) {
  last_progress_ = CalculateProgressState(presentation_time);
}

struct SVGSMILElement::NotifyDependentsInfo {
  explicit NotifyDependentsInfo(const SMILInterval& interval)
      : origin(SMILTimeOrigin::kSyncBase),
        repeat_nr(0),
        begin(interval.begin),
        end(interval.end) {}
  NotifyDependentsInfo(unsigned repeat_nr, SMILTime repeat_time)
      : origin(SMILTimeOrigin::kRepeat),
        repeat_nr(repeat_nr),
        begin(repeat_time),
        end(SMILTime::Unresolved()) {}

  SMILTimeOrigin origin;
  unsigned repeat_nr;
  SMILTime begin;  // repeat time if origin == kRepeat
  SMILTime end;
};

void SVGSMILElement::NotifyDependentsOnNewInterval(
    const SMILInterval& interval) {
  DCHECK(interval.IsResolved());
  NotifyDependents(NotifyDependentsInfo(interval));
}

void SVGSMILElement::NotifyDependentsOnRepeat(unsigned repeat_nr,
                                              SMILTime repeat_time) {
  DCHECK(repeat_nr);
  DCHECK(repeat_time.IsFinite());
  NotifyDependents(NotifyDependentsInfo(repeat_nr, repeat_time));
}

void SVGSMILElement::NotifyDependents(const NotifyDependentsInfo& info) {
  // Avoid infinite recursion which may be caused by:
  // |NotifyDependents| -> |CreateInstanceTimesFromSyncBase| ->
  // |AddInstanceTime| -> |InstanceListChanged| -> |NotifyDependents|
  if (is_notifying_dependents_)
    return;
  base::AutoReset<bool> reentrancy_guard(&is_notifying_dependents_, true);
  for (SVGSMILElement* element : sync_base_dependents_)
    element->CreateInstanceTimesFromSyncBase(this, info);
}

void SVGSMILElement::CreateInstanceTimesFromSyncBase(
    SVGSMILElement* timed_element,
    const NotifyDependentsInfo& info) {
  // FIXME: To be really correct, this should handle updating exising interval
  // by changing the associated times instead of creating new ones.
  for (Condition* condition : conditions_) {
    if (!condition->IsSyncBaseFor(timed_element))
      continue;
    // TODO(edvardt): This is a lot of string compares, which is slow, it
    // might be a good idea to change it for an enum and maybe make Condition
    // into a union?
    DCHECK(condition->GetName() == "begin" || condition->GetName() == "end" ||
           condition->GetName() == "repeat");

    // No nested time containers in SVG, no need for crazy time space
    // conversions. Phew!
    SMILTime time = SMILTime::Unresolved();
    if (info.origin == SMILTimeOrigin::kSyncBase) {
      if (condition->GetName() == "begin") {
        time = info.begin + condition->Offset();
      } else if (condition->GetName() == "end") {
        time = info.end + condition->Offset();
      }
    } else {
      DCHECK_EQ(info.origin, SMILTimeOrigin::kRepeat);
      if (info.repeat_nr != condition->Repeat())
        continue;
      time = info.begin + condition->Offset();
    }
    if (!time.IsFinite())
      continue;
    AddInstanceTime(condition->GetBeginOrEnd(), time, info.origin);
  }

  // No instance times were added.
  if (!instance_lists_have_changed_)
    return;

  // We're currently sending notifications for, and thus updating, this element
  // so let that update handle the new instance times.
  if (is_notifying_dependents_)
    return;

  InstanceListChanged();
}

void SVGSMILElement::AddSyncBaseDependent(SVGSMILElement& animation) {
  sync_base_dependents_.insert(&animation);
  if (!interval_.IsResolved())
    return;
  animation.CreateInstanceTimesFromSyncBase(this,
                                            NotifyDependentsInfo(interval_));
}

void SVGSMILElement::RemoveSyncBaseDependent(SVGSMILElement& animation) {
  sync_base_dependents_.erase(&animation);
}

void SVGSMILElement::BeginByLinkActivation() {
  AddInstanceTimeAndUpdate(kBegin, Elapsed(), SMILTimeOrigin::kLinkActivation);
}

void SVGSMILElement::StartedActiveInterval() {
  is_waiting_for_first_interval_ = false;
}

void SVGSMILElement::EndedActiveInterval() {
  begin_times_.RemoveWithOrigin(SMILTimeOrigin::kScript);
  end_times_.RemoveWithOrigin(SMILTimeOrigin::kScript);
}

bool SVGSMILElement::HasValidTarget() const {
  return targetElement() && targetElement()->InActiveDocument();
}

void SVGSMILElement::WillChangeAnimationTarget() {
  if (!is_scheduled_)
    return;
  DCHECK(time_container_);
  DCHECK(target_element_);
  time_container_->Unschedule(this);
  RemovedFromTimeContainer();
  is_scheduled_ = false;
}

void SVGSMILElement::DidChangeAnimationTarget() {
  DCHECK(!is_scheduled_);
  if (!time_container_ || !HasValidTarget())
    return;
  time_container_->Schedule(this);
  AddedToTimeContainer();
  is_scheduled_ = true;
}

void SVGSMILElement::Trace(Visitor* visitor) const {
  visitor->Trace(target_element_);
  visitor->Trace(target_id_observer_);
  visitor->Trace(time_container_);
  visitor->Trace(conditions_);
  visitor->Trace(sync_base_dependents_);
  SVGElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

}  // namespace blink
