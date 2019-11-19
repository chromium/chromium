/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/html_marquee_element.h"

#include <cstdlib>

#include "third_party/blink/renderer/bindings/core/v8/v8_html_marquee_element.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_options.h"
#include "third_party/blink/renderer/core/animation/optional_effect_timing.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLMarqueeElement::HTMLMarqueeElement(Document& document)
    : HTMLElement(html_names::kMarqueeTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLMarqueeElement);
  EnsureUserAgentShadowRoot();
}

void HTMLMarqueeElement::DidAddUserAgentShadowRoot(ShadowRoot& shadow_root) {
  auto* style = MakeGarbageCollected<HTMLStyleElement>(GetDocument(),
                                                       CreateElementFlags());
  style->setTextContent(
      ":host { display: inline-block; overflow: hidden;"
      "text-align: initial; white-space: nowrap; }"
      ":host([direction=\"up\"]), :host([direction=\"down\"]) { overflow: "
      "initial; overflow-y: hidden; white-space: initial; }"
      ":host > div { will-change: transform; }");
  shadow_root.AppendChild(style);

  auto* mover = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  shadow_root.AppendChild(mover);

  mover->AppendChild(
      HTMLSlotElement::CreateUserAgentDefaultSlot(GetDocument()));
  mover_ = mover;
}

class HTMLMarqueeElement::RequestAnimationFrameCallback final
    : public FrameRequestCallbackCollection::FrameCallback {
 public:
  explicit RequestAnimationFrameCallback(HTMLMarqueeElement* marquee)
      : marquee_(marquee) {}

  void Invoke(double) override {
    marquee_->continue_callback_request_id_ = 0;
    marquee_->ContinueAnimation();
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(marquee_);
    FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
  }

 private:
  Member<HTMLMarqueeElement> marquee_;

  DISALLOW_COPY_AND_ASSIGN(RequestAnimationFrameCallback);
};

class HTMLMarqueeElement::AnimationFinished final : public NativeEventListener {
 public:
  explicit AnimationFinished(HTMLMarqueeElement* marquee) : marquee_(marquee) {}

  void Invoke(ExecutionContext*, Event*) override {
    ++marquee_->loop_count_;
    marquee_->start();
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(marquee_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<HTMLMarqueeElement> marquee_;
};

Node::InsertionNotificationRequest HTMLMarqueeElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);

  if (isConnected())
    start();

  return kInsertionDone;
}

void HTMLMarqueeElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (insertion_point.isConnected()) {
    stop();
  }
}

bool HTMLMarqueeElement::IsHorizontal() const {
  Direction direction = GetDirection();
  return direction != kUp && direction != kDown;
}

unsigned HTMLMarqueeElement::scrollAmount() const {
  unsigned scroll_amount = 0;
  AtomicString value = FastGetAttribute(html_names::kScrollamountAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, scroll_amount) ||
      scroll_amount > 0x7fffffffu)
    return kDefaultScrollAmount;
  return scroll_amount;
}

void HTMLMarqueeElement::setScrollAmount(unsigned value) {
  SetUnsignedIntegralAttribute(html_names::kScrollamountAttr, value,
                               kDefaultScrollAmount);
}

unsigned HTMLMarqueeElement::scrollDelay() const {
  unsigned scroll_delay = 0;
  AtomicString value = FastGetAttribute(html_names::kScrolldelayAttr);
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, scroll_delay) ||
      scroll_delay > 0x7fffffffu)
    return kDefaultScrollDelayMS;
  return scroll_delay;
}

void HTMLMarqueeElement::setScrollDelay(unsigned value) {
  SetUnsignedIntegralAttribute(html_names::kScrolldelayAttr, value,
                               kDefaultScrollDelayMS);
}

int HTMLMarqueeElement::loop() const {
  bool ok;
  int loop = FastGetAttribute(html_names::kLoopAttr).ToInt(&ok);
  if (!ok || loop <= 0)
    return kDefaultLoopLimit;
  return loop;
}

void HTMLMarqueeElement::setLoop(int value, ExceptionState& exception_state) {
  if (value <= 0 && value != -1) {
    exception_state.ThrowDOMException(DOMExceptionCode::kIndexSizeError,
                                      "The provided value (" +
                                          String::Number(value) +
                                          ") is neither positive nor -1.");
    return;
  }
  SetIntegralAttribute(html_names::kLoopAttr, value);
}

void HTMLMarqueeElement::start() {
  if (continue_callback_request_id_)
    return;

  RequestAnimationFrameCallback* callback =
      MakeGarbageCollected<RequestAnimationFrameCallback>(this);
  continue_callback_request_id_ = GetDocument().RequestAnimationFrame(callback);
}

void HTMLMarqueeElement::stop() {
  if (continue_callback_request_id_) {
    GetDocument().CancelAnimationFrame(continue_callback_request_id_);
    continue_callback_request_id_ = 0;
    return;
  }

  if (player_)
    player_->pause();
}

bool HTMLMarqueeElement::IsPresentationAttribute(
    const QualifiedName& attr) const {
  if (attr == html_names::kBgcolorAttr || attr == html_names::kHeightAttr ||
      attr == html_names::kHspaceAttr || attr == html_names::kVspaceAttr ||
      attr == html_names::kWidthAttr) {
    return true;
  }
  return HTMLElement::IsPresentationAttribute(attr);
}

void HTMLMarqueeElement::CollectStyleForPresentationAttribute(
    const QualifiedName& attr,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (attr == html_names::kBgcolorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyID::kBackgroundColor, value);
  } else if (attr == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else if (attr == html_names::kHspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginRight, value);
  } else if (attr == html_names::kVspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginBottom, value);
  } else if (attr == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(attr, value, style);
  }
}

StringKeyframeEffectModel* HTMLMarqueeElement::CreateEffectModel(
    const AnimationParameters& parameters) {
  StyleSheetContents* style_sheet_contents =
      mover_->GetDocument().ElementSheet().Contents();
  MutableCSSPropertyValueSet::SetResult set_result;

  SecureContextMode secure_context_mode =
      mover_->GetDocument().GetSecureContextMode();

  StringKeyframeVector keyframes;
  auto* keyframe1 = MakeGarbageCollected<StringKeyframe>();
  set_result = keyframe1->SetCSSPropertyValue(
      CSSPropertyID::kTransform, parameters.transform_begin,
      secure_context_mode, style_sheet_contents);
  DCHECK(set_result.did_parse);
  keyframes.push_back(keyframe1);

  auto* keyframe2 = MakeGarbageCollected<StringKeyframe>();
  set_result = keyframe2->SetCSSPropertyValue(
      CSSPropertyID::kTransform, parameters.transform_end, secure_context_mode,
      style_sheet_contents);
  DCHECK(set_result.did_parse);
  keyframes.push_back(keyframe2);

  return MakeGarbageCollected<StringKeyframeEffectModel>(
      keyframes, EffectModel::kCompositeReplace,
      LinearTimingFunction::Shared());
}

void HTMLMarqueeElement::ContinueAnimation() {
  if (!ShouldContinue())
    return;

  if (player_ && player_->playState() == "paused") {
    player_->play();
    return;
  }

  AnimationParameters parameters = GetAnimationParameters();
  int scroll_delay = scrollDelay();
  int scroll_amount = scrollAmount();

  if (scroll_delay < kMinimumScrollDelayMS &&
      !FastHasAttribute(html_names::kTruespeedAttr))
    scroll_delay = kDefaultScrollDelayMS;
  double duration = 0;
  if (scroll_amount)
    duration = parameters.distance * scroll_delay / scroll_amount;
  if (duration <= 0)
    return;

  StringKeyframeEffectModel* effect_model = CreateEffectModel(parameters);
  Timing timing;
  OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
  effect_timing->setFill("forwards");
  effect_timing->setDuration(
      UnrestrictedDoubleOrString::FromUnrestrictedDouble(duration));
  TimingInput::Update(timing, effect_timing, nullptr, ASSERT_NO_EXCEPTION);

  auto* keyframe_effect =
      MakeGarbageCollected<KeyframeEffect>(mover_, effect_model, timing);
  Animation* player = mover_->GetDocument().Timeline().Play(keyframe_effect);
  player->setId(g_empty_string);
  player->setOnfinish(MakeGarbageCollected<AnimationFinished>(this));

  player_ = player;
}

bool HTMLMarqueeElement::ShouldContinue() {
  int loop_count = loop();

  // By default, slide loops only once.
  if (loop_count <= 0 && GetBehavior() == kSlide)
    loop_count = 1;

  if (loop_count <= 0)
    return true;
  return loop_count_ < loop_count;
}

HTMLMarqueeElement::Behavior HTMLMarqueeElement::GetBehavior() const {
  const AtomicString& behavior = FastGetAttribute(html_names::kBehaviorAttr);
  if (EqualIgnoringASCIICase(behavior, "alternate"))
    return kAlternate;
  if (EqualIgnoringASCIICase(behavior, "slide"))
    return kSlide;
  return kScroll;
}

HTMLMarqueeElement::Direction HTMLMarqueeElement::GetDirection() const {
  const AtomicString& direction = FastGetAttribute(html_names::kDirectionAttr);
  if (EqualIgnoringASCIICase(direction, "down"))
    return kDown;
  if (EqualIgnoringASCIICase(direction, "up"))
    return kUp;
  if (EqualIgnoringASCIICase(direction, "right"))
    return kRight;
  return kLeft;
}

HTMLMarqueeElement::Metrics HTMLMarqueeElement::GetMetrics() {
  Metrics metrics;
  CSSStyleDeclaration* marquee_style =
      GetDocument().domWindow()->getComputedStyle(this);
  // For marquees that are declared inline, getComputedStyle returns "auto" for
  // width and height. Setting all the metrics to zero disables animation for
  // inline marquees.
  if (marquee_style->getPropertyValue("width") == "auto" &&
      marquee_style->getPropertyValue("height") == "auto") {
    metrics.content_height = 0;
    metrics.content_width = 0;
    metrics.marquee_width = 0;
    metrics.marquee_height = 0;
    return metrics;
  }

  if (IsHorizontal()) {
    mover_->style()->setProperty(&GetDocument(), "width", "-webkit-max-content",
                                 "important", ASSERT_NO_EXCEPTION);
  } else {
    mover_->style()->setProperty(&GetDocument(), "height",
                                 "-webkit-max-content", "important",
                                 ASSERT_NO_EXCEPTION);
  }
  CSSStyleDeclaration* mover_style =
      GetDocument().domWindow()->getComputedStyle(mover_);

  metrics.content_width = mover_style->getPropertyValue("width").ToDouble();
  metrics.content_height = mover_style->getPropertyValue("height").ToDouble();
  metrics.marquee_width = marquee_style->getPropertyValue("width").ToDouble();
  metrics.marquee_height = marquee_style->getPropertyValue("height").ToDouble();

  if (IsHorizontal()) {
    mover_->style()->removeProperty("width", ASSERT_NO_EXCEPTION);
  } else {
    mover_->style()->removeProperty("height", ASSERT_NO_EXCEPTION);
  }

  return metrics;
}

HTMLMarqueeElement::AnimationParameters
HTMLMarqueeElement::GetAnimationParameters() {
  AnimationParameters parameters;
  Metrics metrics = GetMetrics();

  double total_width = metrics.marquee_width + metrics.content_width;
  double total_height = metrics.marquee_height + metrics.content_height;

  double inner_width = metrics.marquee_width - metrics.content_width;
  double inner_height = metrics.marquee_height - metrics.content_height;

  switch (GetBehavior()) {
    case kAlternate:
      switch (GetDirection()) {
        case kRight:
          parameters.transform_begin =
              CreateTransform(inner_width >= 0 ? 0 : inner_width);
          parameters.transform_end =
              CreateTransform(inner_width >= 0 ? inner_width : 0);
          parameters.distance = std::abs(inner_width);
          break;
        case kUp:
          parameters.transform_begin =
              CreateTransform(inner_height >= 0 ? inner_height : 0);
          parameters.transform_end =
              CreateTransform(inner_height >= 0 ? 0 : inner_height);
          parameters.distance = std::abs(inner_height);
          break;
        case kDown:
          parameters.transform_begin =
              CreateTransform(inner_height >= 0 ? 0 : inner_height);
          parameters.transform_end =
              CreateTransform(inner_height >= 0 ? inner_height : 0);
          parameters.distance = std::abs(inner_height);
          break;
        case kLeft:
        default:
          parameters.transform_begin =
              CreateTransform(inner_width >= 0 ? inner_width : 0);
          parameters.transform_end =
              CreateTransform(inner_width >= 0 ? 0 : inner_width);
          parameters.distance = std::abs(inner_width);
      }

      if (loop_count_ % 2)
        std::swap(parameters.transform_begin, parameters.transform_end);
      break;
    case kSlide:
      switch (GetDirection()) {
        case kRight:
          parameters.transform_begin = CreateTransform(-metrics.content_width);
          parameters.transform_end = CreateTransform(inner_width);
          parameters.distance = metrics.marquee_width;
          break;
        case kUp:
          parameters.transform_begin = CreateTransform(metrics.marquee_height);
          parameters.transform_end = "translateY(0)";
          parameters.distance = metrics.marquee_height;
          break;
        case kDown:
          parameters.transform_begin = CreateTransform(-metrics.content_height);
          parameters.transform_end = CreateTransform(inner_height);
          parameters.distance = metrics.marquee_height;
          break;
        case kLeft:
        default:
          parameters.transform_begin = CreateTransform(metrics.marquee_width);
          parameters.transform_end = "translateX(0)";
          parameters.distance = metrics.marquee_width;
      }
      break;
    case kScroll:
    default:
      switch (GetDirection()) {
        case kRight:
          parameters.transform_begin = CreateTransform(-metrics.content_width);
          parameters.transform_end = CreateTransform(metrics.marquee_width);
          parameters.distance = total_width;
          break;
        case kUp:
          parameters.transform_begin = CreateTransform(metrics.marquee_height);
          parameters.transform_end = CreateTransform(-metrics.content_height);
          parameters.distance = total_height;
          break;
        case kDown:
          parameters.transform_begin = CreateTransform(-metrics.content_height);
          parameters.transform_end = CreateTransform(metrics.marquee_height);
          parameters.distance = total_height;
          break;
        case kLeft:
        default:
          parameters.transform_begin = CreateTransform(metrics.marquee_width);
          parameters.transform_end = CreateTransform(-metrics.content_width);
          parameters.distance = total_width;
      }
      break;
  }

  return parameters;
}

AtomicString HTMLMarqueeElement::CreateTransform(double value) const {
  char axis = IsHorizontal() ? 'X' : 'Y';
  return String::Format("translate%c(", axis) +
         String::NumberToStringECMAScript(value) + "px)";
}

void HTMLMarqueeElement::Trace(Visitor* visitor) {
  visitor->Trace(mover_);
  visitor->Trace(player_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
