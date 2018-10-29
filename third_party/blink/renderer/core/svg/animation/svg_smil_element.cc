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

#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"

#include <algorithm>
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class RepeatEvent final : public Event {
 public:
  static RepeatEvent* Create(const AtomicString& type, int repeat) {
    return new RepeatEvent(type, Bubbles::kNo, Cancelable::kNo, repeat);
  }

  ~RepeatEvent() override = default;

  int Repeat() const { return repeat_; }

  void Trace(blink::Visitor* visitor) override { Event::Trace(visitor); }

 protected:
  RepeatEvent(const AtomicString& type,
              Bubbles bubbles,
              Cancelable cancelable,
              int repeat = -1)
      : Event(type, bubbles, cancelable), repeat_(repeat) {}

 private:
  int repeat_;
};

inline RepeatEvent* ToRepeatEvent(Event* event) {
  SECURITY_DCHECK(!event || event->type() == "repeatn");
  return static_cast<RepeatEvent*>(event);
}

// This is used for duration type time values that can't be negative.
static const double kInvalidCachedTime = -1.;

class ConditionEventListener final : public EventListener {
 public:
  static ConditionEventListener* Create(SVGSMILElement* animation,
                                        SVGSMILElement::Condition* condition) {
    return new ConditionEventListener(animation, condition);
  }

  static const ConditionEventListener* Cast(const EventListener* listener) {
    return listener->GetType() == kConditionEventListenerType
               ? static_cast<const ConditionEventListener*>(listener)
               : nullptr;
  }

  bool operator==(const EventListener& other) const override;

  void DisconnectAnimation() { animation_ = nullptr; }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(animation_);
    visitor->Trace(condition_);
    EventListener::Trace(visitor);
  }

 private:
  ConditionEventListener(SVGSMILElement* animation,
                         SVGSMILElement::Condition* condition)
      : EventListener(kConditionEventListenerType),
        animation_(animation),
        condition_(condition) {}

  void handleEvent(ExecutionContext*, Event*) override;

  Member<SVGSMILElement> animation_;
  Member<SVGSMILElement::Condition> condition_;
};

bool ConditionEventListener::operator==(const EventListener& listener) const {
  if (const ConditionEventListener* condition_event_listener =
          ConditionEventListener::Cast(&listener))
    return animation_ == condition_event_listener->animation_ &&
           condition_ == condition_event_listener->condition_;
  return false;
}

void ConditionEventListener::handleEvent(ExecutionContext*, Event* event) {
  if (!animation_)
    return;
  if (event->type() == "repeatn" &&
      ToRepeatEvent(event)->Repeat() != condition_->Repeat())
    return;
  animation_->AddInstanceTime(condition_->GetBeginOrEnd(),
                              animation_->Elapsed() + condition_->Offset());
}

SVGSMILElement::Condition::Condition(Type type,
                                     BeginOrEnd begin_or_end,
                                     const AtomicString& base_id,
                                     const AtomicString& name,
                                     SMILTime offset,
                                     int repeat)
    : type_(type),
      begin_or_end_(begin_or_end),
      base_id_(base_id),
      name_(name),
      offset_(offset),
      repeat_(repeat) {}

SVGSMILElement::Condition::~Condition() = default;

void SVGSMILElement::Condition::Trace(blink::Visitor* visitor) {
  visitor->Trace(base_element_);
  visitor->Trace(base_id_observer_);
  visitor->Trace(event_listener_);
}

void SVGSMILElement::Condition::ConnectSyncBase(SVGSMILElement& timed_element) {
  DCHECK(!base_id_.IsEmpty());
  DCHECK_EQ(type_, kSyncbase);
  Element* element = timed_element.GetTreeScope().getElementById(base_id_);
  if (!element || !IsSVGSMILElement(*element)) {
    base_element_ = nullptr;
    return;
  }
  base_element_ = ToSVGSMILElement(element);
  ToSVGSMILElement(*element).AddSyncBaseDependent(timed_element);
}

void SVGSMILElement::Condition::DisconnectSyncBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kSyncbase);
  if (!base_element_)
    return;
  ToSVGSMILElement(*base_element_).RemoveSyncBaseDependent(timed_element);
  base_element_ = nullptr;
}

void SVGSMILElement::Condition::ConnectEventBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kEventBase);
  DCHECK(!base_element_);
  Element* target;
  if (base_id_.IsEmpty()) {
    target = timed_element.targetElement();
  } else {
    target = SVGURIReference::ObserveTarget(
        base_id_observer_, timed_element.GetTreeScope(), base_id_,
        WTF::BindRepeating(&SVGSMILElement::BuildPendingResource,
                           WrapWeakPersistent(&timed_element)));
  }
  if (!target || !target->IsSVGElement())
    return;
  DCHECK(!event_listener_);
  event_listener_ = ConditionEventListener::Create(&timed_element, this);
  base_element_ = ToSVGElement(target);
  base_element_->addEventListener(name_, event_listener_, false);
  timed_element.AddReferenceTo(base_element_);
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
      attribute_name_(AnyQName()),
      target_element_(nullptr),
      sync_base_conditions_connected_(false),
      has_end_event_conditions_(false),
      is_waiting_for_first_interval_(true),
      is_scheduled_(false),
      interval_(SMILInterval(SMILTime::Unresolved(), SMILTime::Unresolved())),
      previous_interval_begin_(SMILTime::Unresolved()),
      active_state_(kInactive),
      restart_(kRestartAlways),
      fill_(kFillRemove),
      last_percent_(0),
      last_repeat_(0),
      next_progress_time_(0),
      document_order_index_(0),
      cached_dur_(kInvalidCachedTime),
      cached_repeat_dur_(kInvalidCachedTime),
      cached_repeat_count_(kInvalidCachedTime),
      cached_min_(kInvalidCachedTime),
      cached_max_(kInvalidCachedTime) {
  ResolveFirstInterval();
}

SVGSMILElement::~SVGSMILElement() = default;

void SVGSMILElement::ClearResourceAndEventBaseReferences() {
  SVGURIReference::UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGSMILElement::ClearConditions() {
  DisconnectSyncBaseConditions();
  DisconnectEventBaseConditions();
  conditions_.clear();
}

void SVGSMILElement::BuildPendingResource() {
  ClearResourceAndEventBaseReferences();
  DisconnectEventBaseConditions();

  if (!isConnected()) {
    // Reset the target element if we are no longer in the document.
    SetTargetElement(nullptr);
    return;
  }

  const AtomicString& href = SVGURIReference::LegacyHrefString(*this);
  Element* target;
  if (href.IsEmpty()) {
    target = parentElement();
  } else {
    target = SVGURIReference::ObserveTarget(target_id_observer_, *this, href);
  }
  SVGElement* svg_target =
      target && target->IsSVGElement() ? ToSVGElement(target) : nullptr;

  if (svg_target && !svg_target->isConnected())
    svg_target = nullptr;

  if (svg_target != targetElement())
    SetTargetElement(svg_target);

  if (svg_target) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(svg_target);
  }
  ConnectEventBaseConditions();
}

static inline void ClearTimesWithDynamicOrigins(
    Vector<SMILTimeWithOrigin>& time_list) {
  for (int i = time_list.size() - 1; i >= 0; --i) {
    if (time_list[i].OriginIsScript())
      time_list.EraseAt(i);
  }
}

void SVGSMILElement::Reset() {
  ClearAnimatedType();

  active_state_ = kInactive;
  is_waiting_for_first_interval_ = true;
  interval_.begin = SMILTime::Unresolved();
  interval_.end = SMILTime::Unresolved();
  previous_interval_begin_ = SMILTime::Unresolved();
  last_percent_ = 0;
  last_repeat_ = 0;
  next_progress_time_ = 0;
  ResolveFirstInterval();
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
  if (!FastHasAttribute(svg_names::kBeginAttr))
    begin_times_.push_back(SMILTimeWithOrigin());

  if (is_waiting_for_first_interval_)
    ResolveFirstInterval();

  if (time_container_)
    time_container_->NotifyIntervalsChanged();

  BuildPendingResource();

  return kInsertionDone;
}

void SVGSMILElement::RemovedFrom(ContainerNode& root_parent) {
  if (root_parent.isConnected()) {
    ClearResourceAndEventBaseReferences();
    ClearConditions();
    SetTargetElement(nullptr);
    AnimationAttributeChanged();
    time_container_ = nullptr;
  }

  SVGElement::RemovedFrom(root_parent);
}

SMILTime SVGSMILElement::ParseOffsetValue(const String& data) {
  bool ok;
  double result = 0;
  String parse = data.StripWhiteSpace();
  if (parse.EndsWith('h'))
    result = parse.Left(parse.length() - 1).ToDouble(&ok) * 60 * 60;
  else if (parse.EndsWith("min"))
    result = parse.Left(parse.length() - 3).ToDouble(&ok) * 60;
  else if (parse.EndsWith("ms"))
    result = parse.Left(parse.length() - 2).ToDouble(&ok) / 1000;
  else if (parse.EndsWith('s'))
    result = parse.Left(parse.length() - 1).ToDouble(&ok);
  else
    result = parse.ToDouble(&ok);
  if (!ok || !SMILTime(result).IsFinite())
    return SMILTime::Unresolved();
  return result;
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
  } else
    return ParseOffsetValue(parse);

  if (!ok || !SMILTime(result).IsFinite())
    return SMILTime::Unresolved();
  return result;
}

static void SortTimeList(Vector<SMILTimeWithOrigin>& time_list) {
  std::sort(time_list.begin(), time_list.end());
}

bool SVGSMILElement::ParseCondition(const String& value,
                                    BeginOrEnd begin_or_end) {
  String parse_string = value.StripWhiteSpace();

  double sign = 1.;
  bool ok;
  wtf_size_t pos = parse_string.find('+');
  if (pos == kNotFound) {
    pos = parse_string.find('-');
    if (pos != kNotFound)
      sign = -1.;
  }
  String condition_string;
  SMILTime offset = 0;
  if (pos == kNotFound)
    condition_string = parse_string;
  else {
    condition_string = parse_string.Left(pos).StripWhiteSpace();
    String offset_string = parse_string.Substring(pos + 1).StripWhiteSpace();
    offset = ParseOffsetValue(offset_string);
    if (offset.IsUnresolved())
      return false;
    offset = offset * sign;
  }
  if (condition_string.IsEmpty())
    return false;
  pos = condition_string.find('.');

  String base_id;
  String name_string;
  if (pos == kNotFound)
    name_string = condition_string;
  else {
    base_id = condition_string.Left(pos);
    name_string = condition_string.Substring(pos + 1);
  }
  if (name_string.IsEmpty())
    return false;

  Condition::Type type;
  int repeat = -1;
  if (name_string.StartsWith("repeat(") && name_string.EndsWith(')')) {
    repeat =
        name_string.Substring(7, name_string.length() - 8).ToUIntStrict(&ok);
    if (!ok)
      return false;
    name_string = "repeatn";
    type = Condition::kEventBase;
  } else if (name_string == "begin" || name_string == "end") {
    if (base_id.IsEmpty())
      return false;
    UseCounter::Count(&GetDocument(),
                      WebFeature::kSVGSMILBeginOrEndSyncbaseValue);
    type = Condition::kSyncbase;
  } else if (name_string.StartsWith("accesskey(")) {
    // FIXME: accesskey() support.
    type = Condition::kAccessKey;
  } else {
    UseCounter::Count(&GetDocument(), WebFeature::kSVGSMILBeginOrEndEventValue);
    type = Condition::kEventBase;
  }

  conditions_.push_back(
      Condition::Create(type, begin_or_end, AtomicString(base_id),
                        AtomicString(name_string), offset, repeat));

  if (type == Condition::kEventBase && begin_or_end == kEnd)
    has_end_event_conditions_ = true;

  return true;
}

void SVGSMILElement::ParseBeginOrEnd(const String& parse_string,
                                     BeginOrEnd begin_or_end) {
  Vector<SMILTimeWithOrigin>& time_list =
      begin_or_end == kBegin ? begin_times_ : end_times_;
  if (begin_or_end == kEnd)
    has_end_event_conditions_ = false;
  HashSet<SMILTime> existing;
  for (unsigned n = 0; n < time_list.size(); ++n) {
    if (!time_list[n].Time().IsUnresolved())
      existing.insert(time_list[n].Time().Value());
  }
  Vector<String> split_string;
  parse_string.Split(';', split_string);
  for (unsigned n = 0; n < split_string.size(); ++n) {
    SMILTime value = ParseClockValue(split_string[n]);
    if (value.IsUnresolved())
      ParseCondition(split_string[n], begin_or_end);
    else if (!existing.Contains(value.Value()))
      time_list.push_back(
          SMILTimeWithOrigin(value, SMILTimeWithOrigin::kParserOrigin));
  }
  SortTimeList(time_list);
}

void SVGSMILElement::ParseAttribute(const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == svg_names::kBeginAttr) {
    if (!conditions_.IsEmpty()) {
      ClearConditions();
      ParseBeginOrEnd(FastGetAttribute(svg_names::kEndAttr), kEnd);
    }
    ParseBeginOrEnd(value.GetString(), kBegin);
    if (isConnected()) {
      ConnectSyncBaseConditions();
      ConnectEventBaseConditions();
      BeginListChanged(Elapsed());
    }
    AnimationAttributeChanged();
  } else if (name == svg_names::kEndAttr) {
    if (!conditions_.IsEmpty()) {
      ClearConditions();
      ParseBeginOrEnd(FastGetAttribute(svg_names::kBeginAttr), kBegin);
    }
    ParseBeginOrEnd(value.GetString(), kEnd);
    if (isConnected()) {
      ConnectSyncBaseConditions();
      ConnectEventBaseConditions();
      EndListChanged(Elapsed());
    }
    AnimationAttributeChanged();
  } else if (name == svg_names::kOnbeginAttr) {
    SetAttributeEventListener(EventTypeNames::beginEvent,
                              CreateAttributeEventListener(this, name, value));
  } else if (name == svg_names::kOnendAttr) {
    SetAttributeEventListener(EventTypeNames::endEvent,
                              CreateAttributeEventListener(this, name, value));
  } else if (name == svg_names::kOnrepeatAttr) {
    SetAttributeEventListener(EventTypeNames::repeatEvent,
                              CreateAttributeEventListener(this, name, value));
  } else if (name == svg_names::kRestartAttr) {
    if (value == "never")
      restart_ = kRestartNever;
    else if (value == "whenNotActive")
      restart_ = kRestartWhenNotActive;
    else
      restart_ = kRestartAlways;
  } else if (name == svg_names::kFillAttr) {
    fill_ = value == "freeze" ? kFillFreeze : kFillRemove;
  } else {
    SVGElement::ParseAttribute(params);
  }
}

void SVGSMILElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kDurAttr) {
    cached_dur_ = kInvalidCachedTime;
  } else if (attr_name == svg_names::kRepeatDurAttr) {
    cached_repeat_dur_ = kInvalidCachedTime;
  } else if (attr_name == svg_names::kRepeatCountAttr) {
    cached_repeat_count_ = kInvalidCachedTime;
  } else if (attr_name == svg_names::kMinAttr) {
    cached_min_ = kInvalidCachedTime;
  } else if (attr_name == svg_names::kMaxAttr) {
    cached_max_ = kInvalidCachedTime;
  } else if (attr_name.Matches(svg_names::kHrefAttr) ||
             attr_name.Matches(xlink_names::kHrefAttr)) {
    // TODO(fs): Could be smarter here when 'href' is specified and 'xlink:href'
    // is changed.
    SVGElement::InvalidationGuard invalidation_guard(this);
    BuildPendingResource();
  } else {
    SVGElement::SvgAttributeChanged(attr_name);
    return;
  }

  AnimationAttributeChanged();
}

void SVGSMILElement::ConnectSyncBaseConditions() {
  if (sync_base_conditions_connected_)
    DisconnectSyncBaseConditions();
  sync_base_conditions_connected_ = true;
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kSyncbase)
      condition->ConnectSyncBase(*this);
  }
}

void SVGSMILElement::DisconnectSyncBaseConditions() {
  if (!sync_base_conditions_connected_)
    return;
  sync_base_conditions_connected_ = false;
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kSyncbase)
      condition->DisconnectSyncBase(*this);
  }
}

void SVGSMILElement::ConnectEventBaseConditions() {
  DisconnectEventBaseConditions();
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kEventBase)
      condition->ConnectEventBase(*this);
  }
}

void SVGSMILElement::DisconnectEventBaseConditions() {
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kEventBase)
      condition->DisconnectEventBase(*this);
  }
}

void SVGSMILElement::SetTargetElement(SVGElement* target) {
  WillChangeAnimationTarget();

  // Clear values that may depend on the previous target.
  if (target_element_)
    DisconnectSyncBaseConditions();

  // If the animation state is not Inactive, always reset to a clear state
  // before leaving the old target element.
  if (GetActiveState() != kInactive)
    EndedActiveInterval();

  target_element_ = target;
  DidChangeAnimationTarget();

  // If the animation is scheduled and there's an active interval, then
  // revalidate the animation value.
  if (GetActiveState() != kInactive && is_scheduled_)
    StartedActiveInterval();
}

SMILTime SVGSMILElement::Elapsed() const {
  return time_container_ ? time_container_->Elapsed() : 0;
}

bool SVGSMILElement::IsFrozen() const {
  return GetActiveState() == kFrozen;
}

SMILTime SVGSMILElement::Dur() const {
  if (cached_dur_ != kInvalidCachedTime)
    return cached_dur_;
  const AtomicString& value = FastGetAttribute(svg_names::kDurAttr);
  SMILTime clock_value = ParseClockValue(value);
  return cached_dur_ = clock_value <= 0 ? SMILTime::Unresolved() : clock_value;
}

SMILTime SVGSMILElement::RepeatDur() const {
  if (cached_repeat_dur_ != kInvalidCachedTime)
    return cached_repeat_dur_;
  const AtomicString& value = FastGetAttribute(svg_names::kRepeatDurAttr);
  SMILTime clock_value = ParseClockValue(value);
  cached_repeat_dur_ = clock_value <= 0 ? SMILTime::Unresolved() : clock_value;
  return cached_repeat_dur_;
}

// So a count is not really a time but let just all pretend we did not notice.
SMILTime SVGSMILElement::RepeatCount() const {
  if (cached_repeat_count_ != kInvalidCachedTime)
    return cached_repeat_count_;
  SMILTime computed_repeat_count = SMILTime::Unresolved();
  const AtomicString& value = FastGetAttribute(svg_names::kRepeatCountAttr);
  if (!value.IsNull()) {
    DEFINE_STATIC_LOCAL(const AtomicString, indefinite_value, ("indefinite"));
    if (value == indefinite_value) {
      computed_repeat_count = SMILTime::Indefinite();
    } else {
      bool ok;
      double result = value.ToDouble(&ok);
      if (ok && result > 0)
        computed_repeat_count = result;
    }
  }
  cached_repeat_count_ = computed_repeat_count;
  return cached_repeat_count_;
}

SMILTime SVGSMILElement::MaxValue() const {
  if (cached_max_ != kInvalidCachedTime)
    return cached_max_;
  const AtomicString& value = FastGetAttribute(svg_names::kMaxAttr);
  SMILTime result = ParseClockValue(value);
  return cached_max_ = (result.IsUnresolved() || result <= 0)
                           ? SMILTime::Indefinite()
                           : result;
}

SMILTime SVGSMILElement::MinValue() const {
  if (cached_min_ != kInvalidCachedTime)
    return cached_min_;
  const AtomicString& value = FastGetAttribute(svg_names::kMinAttr);
  SMILTime result = ParseClockValue(value);
  return cached_min_ = (result.IsUnresolved() || result < 0) ? 0 : result;
}

SMILTime SVGSMILElement::SimpleDuration() const {
  return std::min(Dur(), SMILTime::Indefinite());
}

void SVGSMILElement::AddInstanceTime(BeginOrEnd begin_or_end,
                                     SMILTime time,
                                     SMILTimeWithOrigin::Origin origin) {
  SMILTime elapsed = this->Elapsed();
  if (elapsed.IsUnresolved())
    return;
  Vector<SMILTimeWithOrigin>& list =
      begin_or_end == kBegin ? begin_times_ : end_times_;
  list.push_back(SMILTimeWithOrigin(time, origin));
  SortTimeList(list);
  if (begin_or_end == kBegin)
    BeginListChanged(elapsed);
  else
    EndListChanged(elapsed);
}

inline bool CompareTimes(const SMILTimeWithOrigin& left,
                         const SMILTimeWithOrigin& right) {
  return left.Time() < right.Time();
}

SMILTime SVGSMILElement::FindInstanceTime(BeginOrEnd begin_or_end,
                                          SMILTime minimum_time,
                                          bool equals_minimum_ok) const {
  const Vector<SMILTimeWithOrigin>& list =
      begin_or_end == kBegin ? begin_times_ : end_times_;
  int size_of_list = list.size();

  if (!size_of_list)
    return begin_or_end == kBegin ? SMILTime::Unresolved()
                                  : SMILTime::Indefinite();

  const SMILTimeWithOrigin dummy_time_with_origin(
      minimum_time, SMILTimeWithOrigin::kParserOrigin);
  const SMILTimeWithOrigin* result = std::lower_bound(
      list.begin(), list.end(), dummy_time_with_origin, CompareTimes);
  int index_of_result = static_cast<int>(result - list.begin());
  if (index_of_result == size_of_list)
    return SMILTime::Unresolved();
  const SMILTime& current_time = list[index_of_result].Time();

  // The special value "indefinite" does not yield an instance time in the begin
  // list.
  if (current_time.IsIndefinite() && begin_or_end == kBegin)
    return SMILTime::Unresolved();

  if (current_time > minimum_time)
    return current_time;

  DCHECK(current_time == minimum_time);
  if (equals_minimum_ok)
    return current_time;

  // If the equals is not accepted, return the next bigger item in the list.
  SMILTime next_time = current_time;
  while (index_of_result < size_of_list - 1) {
    next_time = list[index_of_result + 1].Time();
    if (next_time > minimum_time)
      return next_time;
    ++index_of_result;
  }

  return begin_or_end == kBegin ? SMILTime::Unresolved()
                                : SMILTime::Indefinite();
}

SMILTime SVGSMILElement::RepeatingDuration() const {
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILTime repeat_count = this->RepeatCount();
  SMILTime repeat_dur = this->RepeatDur();
  SMILTime simple_duration = this->SimpleDuration();
  if (!simple_duration ||
      (repeat_dur.IsUnresolved() && repeat_count.IsUnresolved()))
    return simple_duration;
  repeat_dur = std::min(repeat_dur, SMILTime::Indefinite());
  SMILTime repeat_count_duration = simple_duration * repeat_count;
  if (!repeat_count_duration.IsUnresolved())
    return std::min(repeat_dur, repeat_count_duration);
  return repeat_dur;
}

SMILTime SVGSMILElement::ResolveActiveEnd(SMILTime resolved_begin,
                                          SMILTime resolved_end) const {
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILTime preliminary_active_duration;
  if (!resolved_end.IsUnresolved() && Dur().IsUnresolved() &&
      RepeatDur().IsUnresolved() && RepeatCount().IsUnresolved())
    preliminary_active_duration = resolved_end - resolved_begin;
  else if (!resolved_end.IsFinite())
    preliminary_active_duration = RepeatingDuration();
  else
    preliminary_active_duration =
        std::min(RepeatingDuration(), resolved_end - resolved_begin);

  SMILTime min_value = this->MinValue();
  SMILTime max_value = this->MaxValue();
  if (min_value > max_value) {
    // Ignore both.
    // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#MinMax
    min_value = 0;
    max_value = SMILTime::Indefinite();
  }
  return resolved_begin +
         std::min(max_value, std::max(min_value, preliminary_active_duration));
}

SMILInterval SVGSMILElement::ResolveInterval(
    IntervalSelector interval_selector) const {
  bool first = interval_selector == kFirstInterval;
  // See the pseudocode in http://www.w3.org/TR/SMIL3/smil-timing.html#q90.
  SMILTime begin_after =
      first ? -std::numeric_limits<double>::infinity() : interval_.end;
  SMILTime last_interval_temp_end = std::numeric_limits<double>::infinity();
  while (true) {
    bool equals_minimum_ok = !first || interval_.end > interval_.begin;
    SMILTime temp_begin =
        FindInstanceTime(kBegin, begin_after, equals_minimum_ok);
    if (temp_begin.IsUnresolved())
      break;
    SMILTime temp_end;
    if (end_times_.IsEmpty())
      temp_end = ResolveActiveEnd(temp_begin, SMILTime::Indefinite());
    else {
      temp_end = FindInstanceTime(kEnd, temp_begin, true);
      if ((first && temp_begin == temp_end &&
           temp_end == last_interval_temp_end) ||
          (!first && temp_end == interval_.end))
        temp_end = FindInstanceTime(kEnd, temp_begin, false);
      if (temp_end.IsUnresolved()) {
        if (!end_times_.IsEmpty() && !has_end_event_conditions_)
          break;
      }
      temp_end = ResolveActiveEnd(temp_begin, temp_end);
    }
    if (!first || (temp_end > 0 || (!temp_begin.Value() && !temp_end.Value())))
      return SMILInterval(temp_begin, temp_end);

    begin_after = temp_end;
    last_interval_temp_end = temp_end;
  }
  return SMILInterval(SMILTime::Unresolved(), SMILTime::Unresolved());
}

void SVGSMILElement::ResolveFirstInterval() {
  SMILInterval first_interval = ResolveInterval(kFirstInterval);
  DCHECK(!first_interval.begin.IsIndefinite());

  if (!first_interval.begin.IsUnresolved() && first_interval != interval_) {
    interval_ = first_interval;
    NotifyDependentsIntervalChanged();
    next_progress_time_ = next_progress_time_.IsUnresolved()
                              ? interval_.begin
                              : std::min(next_progress_time_, interval_.begin);

    if (time_container_)
      time_container_->NotifyIntervalsChanged();
  }
}

bool SVGSMILElement::ResolveNextInterval() {
  SMILInterval next_interval = ResolveInterval(kNextInterval);
  DCHECK(!next_interval.begin.IsIndefinite());

  if (!next_interval.begin.IsUnresolved() &&
      next_interval.begin != interval_.begin) {
    interval_ = next_interval;
    NotifyDependentsIntervalChanged();
    next_progress_time_ = next_progress_time_.IsUnresolved()
                              ? interval_.begin
                              : std::min(next_progress_time_, interval_.begin);
    return true;
  }

  return false;
}

SMILTime SVGSMILElement::NextProgressTime() const {
  return next_progress_time_;
}

void SVGSMILElement::BeginListChanged(SMILTime event_time) {
  if (is_waiting_for_first_interval_) {
    ResolveFirstInterval();
  } else if (GetRestart() != kRestartNever) {
    SMILTime new_begin = FindInstanceTime(kBegin, event_time, true);
    if (new_begin.IsFinite() &&
        (interval_.end <= event_time || new_begin < interval_.begin)) {
      // Begin time changed, re-resolve the interval.
      SMILTime old_begin = interval_.begin;
      interval_.end = event_time;
      interval_ = ResolveInterval(kNextInterval);
      DCHECK(!interval_.begin.IsUnresolved());
      if (interval_.begin != old_begin) {
        if (GetActiveState() == kActive && interval_.begin > event_time) {
          active_state_ = DetermineActiveState(event_time);
          if (GetActiveState() != kActive)
            EndedActiveInterval();
        }
        NotifyDependentsIntervalChanged();
      }
    }
  }
  next_progress_time_ = Elapsed();

  if (time_container_)
    time_container_->NotifyIntervalsChanged();
}

void SVGSMILElement::EndListChanged(SMILTime) {
  SMILTime elapsed = this->Elapsed();
  if (is_waiting_for_first_interval_) {
    ResolveFirstInterval();
  } else if (elapsed < interval_.end && interval_.begin.IsFinite()) {
    SMILTime new_end = FindInstanceTime(kEnd, interval_.begin, false);
    if (new_end < interval_.end) {
      new_end = ResolveActiveEnd(interval_.begin, new_end);
      if (new_end != interval_.end) {
        interval_.end = new_end;
        NotifyDependentsIntervalChanged();
      }
    }
  }
  next_progress_time_ = elapsed;

  if (time_container_)
    time_container_->NotifyIntervalsChanged();
}

SVGSMILElement::RestartedInterval SVGSMILElement::MaybeRestartInterval(
    double elapsed) {
  DCHECK(!is_waiting_for_first_interval_);
  DCHECK(elapsed >= interval_.begin);

  Restart restart = GetRestart();
  if (restart == kRestartNever)
    return kDidNotRestartInterval;

  if (elapsed < interval_.end) {
    if (restart != kRestartAlways)
      return kDidNotRestartInterval;
    SMILTime next_begin = FindInstanceTime(kBegin, interval_.begin, false);
    if (next_begin < interval_.end) {
      interval_.end = next_begin;
      NotifyDependentsIntervalChanged();
    }
  }

  if (elapsed >= interval_.end) {
    if (ResolveNextInterval() && elapsed >= interval_.begin)
      return kDidRestartInterval;
  }
  return kDidNotRestartInterval;
}

void SVGSMILElement::SeekToIntervalCorrespondingToTime(double elapsed) {
  DCHECK(!is_waiting_for_first_interval_);
  DCHECK(elapsed >= interval_.begin);

  // Manually seek from interval to interval, just as if the animation would run
  // regulary.
  while (true) {
    // Figure out the next value in the begin time list after the current
    // interval begin.
    SMILTime next_begin = FindInstanceTime(kBegin, interval_.begin, false);

    // If the 'nextBegin' time is unresolved (eg. just one defined interval),
    // we're done seeking.
    if (next_begin.IsUnresolved())
      return;

    // If the 'nextBegin' time is larger than or equal to the current interval
    // end time, we're done seeking.  If the 'elapsed' time is smaller than the
    // next begin interval time, we're done seeking.
    if (next_begin < interval_.end && elapsed >= next_begin) {
      // End current interval, and start a new interval from the 'nextBegin'
      // time.
      interval_.end = next_begin;
      if (!ResolveNextInterval())
        break;
      continue;
    }

    // If the desired 'elapsed' time is past the current interval, advance to
    // the next.
    if (elapsed >= interval_.end) {
      if (!ResolveNextInterval())
        break;
      continue;
    }

    return;
  }
}

float SVGSMILElement::CalculateAnimationPercentAndRepeat(
    double elapsed,
    unsigned& repeat) const {
  SMILTime simple_duration = this->SimpleDuration();
  repeat = 0;
  if (simple_duration.IsIndefinite()) {
    repeat = 0;
    return 0.f;
  }
  if (!simple_duration) {
    repeat = 0;
    return 1.f;
  }
  DCHECK(interval_.begin.IsFinite());
  DCHECK(simple_duration.IsFinite());
  double active_time = elapsed - interval_.begin.Value();
  SMILTime repeating_duration = this->RepeatingDuration();
  if (elapsed >= interval_.end || active_time > repeating_duration) {
    repeat = static_cast<unsigned>(repeating_duration.Value() /
                                   simple_duration.Value());
    if (!fmod(repeating_duration.Value(), simple_duration.Value()))
      repeat--;

    // Use the interval to compute the interval position if we've passed the
    // interval end, otherwise use the "repeating duration". This prevents a
    // stale interval (with for instance an 'indefinite' end) from yielding an
    // invalid interval position.
    double last_active_duration =
        elapsed >= interval_.end
            ? interval_.end.Value() - interval_.begin.Value()
            : repeating_duration.Value();
    double percent = last_active_duration / simple_duration.Value();
    percent = percent - floor(percent);
    if (percent < std::numeric_limits<float>::epsilon() ||
        1 - percent < std::numeric_limits<float>::epsilon())
      return 1.0f;
    return clampTo<float>(percent);
  }
  repeat = static_cast<unsigned>(active_time / simple_duration.Value());
  double simple_time = fmod(active_time, simple_duration.Value());
  return clampTo<float>(simple_time / simple_duration.Value());
}

SMILTime SVGSMILElement::CalculateNextProgressTime(double elapsed) const {
  if (GetActiveState() == kActive) {
    // If duration is indefinite the value does not actually change over time.
    // Same is true for <set>.
    SMILTime simple_duration = this->SimpleDuration();
    if (simple_duration.IsIndefinite() || IsSVGSetElement(*this)) {
      SMILTime repeating_duration_end = interval_.begin + RepeatingDuration();
      // We are supposed to do freeze semantics when repeating ends, even if the
      // element is still active.
      // Take care that we get a timer callback at that point.
      if (elapsed < repeating_duration_end &&
          repeating_duration_end < interval_.end &&
          repeating_duration_end.IsFinite())
        return repeating_duration_end;
      return interval_.end;
    }
    return elapsed + 0.025;
  }
  return interval_.begin >= elapsed ? interval_.begin : SMILTime::Unresolved();
}

SVGSMILElement::ActiveState SVGSMILElement::DetermineActiveState(
    SMILTime elapsed) const {
  if (elapsed >= interval_.begin && elapsed < interval_.end)
    return kActive;

  return Fill() == kFillFreeze ? kFrozen : kInactive;
}

bool SVGSMILElement::IsContributing(double elapsed) const {
  // Animation does not contribute during the active time if it is past its
  // repeating duration and has fill=remove.
  return (GetActiveState() == kActive &&
          (Fill() == kFillFreeze ||
           elapsed <= interval_.begin + RepeatingDuration())) ||
         GetActiveState() == kFrozen;
}

bool SVGSMILElement::Progress(double elapsed, bool seek_to_time) {
  DCHECK(time_container_);
  DCHECK(is_waiting_for_first_interval_ || interval_.begin.IsFinite());

  if (!sync_base_conditions_connected_)
    ConnectSyncBaseConditions();

  if (!interval_.begin.IsFinite()) {
    DCHECK_EQ(GetActiveState(), kInactive);
    next_progress_time_ = SMILTime::Unresolved();
    return false;
  }

  if (elapsed < interval_.begin) {
    DCHECK_NE(GetActiveState(), kActive);
    next_progress_time_ = interval_.begin;
    // If the animation is frozen, it's still contributing.
    return GetActiveState() == kFrozen;
  }

  previous_interval_begin_ = interval_.begin;

  if (is_waiting_for_first_interval_) {
    is_waiting_for_first_interval_ = false;
    ResolveFirstInterval();
  }

  // This call may obtain a new interval -- never call
  // calculateAnimationPercentAndRepeat() before!
  if (seek_to_time) {
    SeekToIntervalCorrespondingToTime(elapsed);
    if (elapsed < interval_.begin) {
      // elapsed is not within an interval.
      next_progress_time_ = interval_.begin;
      return false;
    }
  }

  unsigned repeat = 0;
  float percent = CalculateAnimationPercentAndRepeat(elapsed, repeat);
  RestartedInterval restarted_interval = MaybeRestartInterval(elapsed);

  ActiveState old_active_state = GetActiveState();
  active_state_ = DetermineActiveState(elapsed);
  bool animation_is_contributing = IsContributing(elapsed);

  if (animation_is_contributing) {
    if (old_active_state == kInactive ||
        restarted_interval == kDidRestartInterval) {
      ScheduleEvent(EventTypeNames::beginEvent);
      StartedActiveInterval();
    }

    if (repeat && repeat != last_repeat_)
      ScheduleRepeatEvents(repeat);

    last_percent_ = percent;
    last_repeat_ = repeat;
  }

  if ((old_active_state == kActive && GetActiveState() != kActive) ||
      restarted_interval == kDidRestartInterval) {
    ScheduleEvent(EventTypeNames::endEvent);
    EndedActiveInterval();
  }

  // Triggering all the pending events if the animation timeline is changed.
  if (seek_to_time) {
    if (GetActiveState() == kInactive)
      ScheduleEvent(EventTypeNames::beginEvent);

    if (repeat) {
      for (unsigned repeat_event_count = 1; repeat_event_count < repeat;
           repeat_event_count++)
        ScheduleRepeatEvents(repeat_event_count);
      if (GetActiveState() == kInactive)
        ScheduleRepeatEvents(repeat);
    }

    if (GetActiveState() == kInactive || GetActiveState() == kFrozen)
      ScheduleEvent(EventTypeNames::endEvent);
  }

  next_progress_time_ = CalculateNextProgressTime(elapsed);
  return animation_is_contributing;
}

void SVGSMILElement::NotifyDependentsIntervalChanged() {
  DCHECK(interval_.begin.IsFinite());
  // |loopBreaker| is used to avoid infinite recursions which may be caused by:
  // |notifyDependentsIntervalChanged| -> |createInstanceTimesFromSyncbase| ->
  // |add{Begin,End}Time| -> |{begin,end}TimeChanged| ->
  // |notifyDependentsIntervalChanged|
  //
  // As the set here records SVGSMILElements on the stack, it is acceptable to
  // use a HashSet of untraced heap references -- any conservative GC which
  // strikes before unwinding will find these elements on the stack.
  DEFINE_STATIC_LOCAL(HashSet<UntracedMember<SVGSMILElement>>, loop_breaker,
                      ());
  if (!loop_breaker.insert(this).is_new_entry)
    return;

  for (SVGSMILElement* element : sync_base_dependents_)
    element->CreateInstanceTimesFromSyncbase(*this);

  loop_breaker.erase(this);
}

void SVGSMILElement::CreateInstanceTimesFromSyncbase(
    SVGSMILElement& sync_base) {
  // FIXME: To be really correct, this should handle updating exising interval
  // by changing the associated times instead of creating new ones.
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kSyncbase &&
        condition->SyncBaseEquals(sync_base)) {
      DCHECK(condition->GetName() == "begin" || condition->GetName() == "end");
      // No nested time containers in SVG, no need for crazy time space
      // conversions. Phew!
      SMILTime time = 0;
      if (condition->GetName() == "begin")
        time = sync_base.interval_.begin + condition->Offset();
      else
        time = sync_base.interval_.end + condition->Offset();
      if (!time.IsFinite())
        continue;
      AddInstanceTime(condition->GetBeginOrEnd(), time);
    }
  }
}

void SVGSMILElement::AddSyncBaseDependent(SVGSMILElement& animation) {
  sync_base_dependents_.insert(&animation);
  if (interval_.begin.IsFinite())
    animation.CreateInstanceTimesFromSyncbase(*this);
}

void SVGSMILElement::RemoveSyncBaseDependent(SVGSMILElement& animation) {
  sync_base_dependents_.erase(&animation);
}

void SVGSMILElement::BeginByLinkActivation() {
  AddInstanceTime(kBegin, Elapsed());
}

void SVGSMILElement::EndedActiveInterval() {
  ClearTimesWithDynamicOrigins(begin_times_);
  ClearTimesWithDynamicOrigins(end_times_);
}

void SVGSMILElement::ScheduleRepeatEvents(unsigned count) {
  repeat_event_count_list_.push_back(count);
  ScheduleEvent(EventTypeNames::repeatEvent);
  ScheduleEvent(AtomicString("repeatn"));
}

void SVGSMILElement::ScheduleEvent(const AtomicString& event_type) {
  GetDocument()
      .GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(FROM_HERE, WTF::Bind(&SVGSMILElement::DispatchPendingEvent,
                                      WrapPersistent(this), event_type));
}

void SVGSMILElement::DispatchPendingEvent(const AtomicString& event_type) {
  DCHECK(event_type == EventTypeNames::endEvent ||
         event_type == EventTypeNames::beginEvent ||
         event_type == EventTypeNames::repeatEvent || event_type == "repeatn");
  if (event_type == "repeatn") {
    unsigned repeat_event_count = repeat_event_count_list_.front();
    repeat_event_count_list_.EraseAt(0);
    DispatchEvent(*RepeatEvent::Create(event_type, repeat_event_count));
  } else {
    DispatchEvent(*Event::Create(event_type));
  }
}

bool SVGSMILElement::HasValidTarget() {
  return targetElement() && targetElement()->InActiveDocument();
}

void SVGSMILElement::WillChangeAnimationTarget() {
  if (!is_scheduled_)
    return;
  DCHECK(time_container_);
  DCHECK(target_element_);
  time_container_->Unschedule(this, target_element_, attribute_name_);
  is_scheduled_ = false;
}

void SVGSMILElement::DidChangeAnimationTarget() {
  DCHECK(!is_scheduled_);
  if (!time_container_ || !HasValidTarget())
    return;
  time_container_->Schedule(this, target_element_, attribute_name_);
  is_scheduled_ = true;
}

void SVGSMILElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(target_element_);
  visitor->Trace(target_id_observer_);
  visitor->Trace(time_container_);
  visitor->Trace(conditions_);
  visitor->Trace(sync_base_dependents_);
  SVGElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

}  // namespace blink
