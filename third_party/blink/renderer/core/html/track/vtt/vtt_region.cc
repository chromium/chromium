/*
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/vtt/vtt_region.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_parser.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_scanner.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

#define VTT_LOG_LEVEL 3

namespace blink {

namespace {
// The following values default values are defined within the WebVTT Regions
// Spec.
// https://dvcs.w3.org/hg/text-tracks/raw-file/default/608toVTT/region.html

// The region occupies by default 100% of the width of the video viewport.
constexpr double kDefaultRegionWidth = 100;

// The region has, by default, 3 lines of text.
constexpr int kDefaultHeightInLines = 3;

// The region and viewport are anchored in the bottom left corner.
constexpr double kDefaultAnchorPointX = 0;
constexpr double kDefaultAnchorPointY = 100;

// The region doesn't have scrolling text, by default.
constexpr bool kDefaultScroll = false;

// Default region line-height (vh units)
constexpr float kLineHeight = 5.33;

// Default scrolling animation time period (s).
constexpr base::TimeDelta kScrollTime = base::TimeDelta::FromMilliseconds(433);

bool IsNonPercentage(double value,
                     const char* method,
                     ExceptionState& exception_state) {
  if (value < 0 || value > 100) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "value", value, 0.0, ExceptionMessages::kInclusiveBound, 100.0,
            ExceptionMessages::kInclusiveBound));
    return true;
  }
  return false;
}

}  // namespace

VTTRegion::VTTRegion()
    : id_(g_empty_string),
      width_(kDefaultRegionWidth),
      lines_(kDefaultHeightInLines),
      region_anchor_(DoublePoint(kDefaultAnchorPointX, kDefaultAnchorPointY)),
      viewport_anchor_(DoublePoint(kDefaultAnchorPointX, kDefaultAnchorPointY)),
      scroll_(kDefaultScroll),
      current_top_(0),
      scroll_timer_(Thread::Current()->GetTaskRunner(),
                    this,
                    &VTTRegion::ScrollTimerFired) {}

VTTRegion::~VTTRegion() = default;

void VTTRegion::setId(const String& id) {
  id_ = id;
}

void VTTRegion::setWidth(double value, ExceptionState& exception_state) {
  if (IsNonPercentage(value, "width", exception_state))
    return;

  width_ = value;
}

void VTTRegion::setLines(unsigned value) {
  lines_ = value;
}

void VTTRegion::setRegionAnchorX(double value,
                                 ExceptionState& exception_state) {
  if (IsNonPercentage(value, "regionAnchorX", exception_state))
    return;

  region_anchor_.SetX(value);
}

void VTTRegion::setRegionAnchorY(double value,
                                 ExceptionState& exception_state) {
  if (IsNonPercentage(value, "regionAnchorY", exception_state))
    return;

  region_anchor_.SetY(value);
}

void VTTRegion::setViewportAnchorX(double value,
                                   ExceptionState& exception_state) {
  if (IsNonPercentage(value, "viewportAnchorX", exception_state))
    return;

  viewport_anchor_.SetX(value);
}

void VTTRegion::setViewportAnchorY(double value,
                                   ExceptionState& exception_state) {
  if (IsNonPercentage(value, "viewportAnchorY", exception_state))
    return;

  viewport_anchor_.SetY(value);
}

const AtomicString VTTRegion::scroll() const {
  DEFINE_STATIC_LOCAL(const AtomicString, up_scroll_value_keyword, ("up"));
  return scroll_ ? up_scroll_value_keyword : g_empty_atom;
}

void VTTRegion::setScroll(const AtomicString& value) {
  DCHECK(value == "up" || value == g_empty_atom);
  scroll_ = value != g_empty_atom;
}

void VTTRegion::SetRegionSettings(const String& input_string) {
  VTTScanner input(input_string);

  while (!input.IsAtEnd()) {
    input.SkipWhile<VTTParser::IsASpace>();

    if (input.IsAtEnd())
      break;

    // Scan the name part.
    RegionSetting name = ScanSettingName(input);

    // Verify that we're looking at a ':'.
    if (name == kNone || !input.Scan(':')) {
      input.SkipUntil<VTTParser::IsASpace>();
      continue;
    }

    // Scan the value part.
    ParseSettingValue(name, input);
  }
}

VTTRegion::RegionSetting VTTRegion::ScanSettingName(VTTScanner& input) {
  if (input.Scan("id"))
    return kId;
  if (input.Scan("lines"))
    return kLines;
  if (input.Scan("width"))
    return kWidth;
  if (input.Scan("viewportanchor"))
    return kViewportAnchor;
  if (input.Scan("regionanchor"))
    return kRegionAnchor;
  if (input.Scan("scroll"))
    return kScroll;

  return kNone;
}

static inline bool ParsedEntireRun(const VTTScanner& input,
                                   const VTTScanner::Run& run) {
  return input.IsAt(run.end());
}

void VTTRegion::ParseSettingValue(RegionSetting setting, VTTScanner& input) {
  DEFINE_STATIC_LOCAL(const AtomicString, scroll_up_value_keyword, ("up"));

  VTTScanner::Run value_run = input.CollectUntil<VTTParser::IsASpace>();

  switch (setting) {
    case kId: {
      String string_value = input.ExtractString(value_run);
      if (string_value.Find("-->") == kNotFound)
        id_ = string_value;
      break;
    }
    case kWidth: {
      double width;
      if (VTTParser::ParsePercentageValue(input, width) &&
          ParsedEntireRun(input, value_run))
        width_ = width;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid Width";
      break;
    }
    case kLines: {
      unsigned number;
      if (input.ScanDigits(number) && ParsedEntireRun(input, value_run))
        lines_ = number;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid Lines";
      break;
    }
    case kRegionAnchor: {
      DoublePoint anchor;
      if (VTTParser::ParsePercentageValuePair(input, ',', anchor) &&
          ParsedEntireRun(input, value_run))
        region_anchor_ = anchor;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid RegionAnchor";
      break;
    }
    case kViewportAnchor: {
      DoublePoint anchor;
      if (VTTParser::ParsePercentageValuePair(input, ',', anchor) &&
          ParsedEntireRun(input, value_run))
        viewport_anchor_ = anchor;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid ViewportAnchor";
      break;
    }
    case kScroll:
      if (input.ScanRun(value_run, scroll_up_value_keyword))
        scroll_ = true;
      else
        DVLOG(VTT_LOG_LEVEL) << "parseSettingValue, invalid Scroll";
      break;
    case kNone:
      break;
  }

  input.SkipRun(value_run);
}

const AtomicString& VTTRegion::TextTrackCueContainerScrollingClass() {
  DEFINE_STATIC_LOCAL(const AtomicString,
                      track_region_cue_container_scrolling_class,
                      ("scrolling"));

  return track_region_cue_container_scrolling_class;
}

HTMLDivElement* VTTRegion::GetDisplayTree(Document& document) {
  if (!region_display_tree_) {
    region_display_tree_ = MakeGarbageCollected<HTMLDivElement>(document);
    PrepareRegionDisplayTree();
  }

  return region_display_tree_;
}

void VTTRegion::WillRemoveVTTCueBox(VTTCueBox* box) {
  DVLOG(VTT_LOG_LEVEL) << "willRemoveVTTCueBox";
  DCHECK(cue_container_->contains(box));

  double box_height = box->getBoundingClientRect()->height();

  cue_container_->classList().Remove(TextTrackCueContainerScrollingClass());

  current_top_ += box_height;
  cue_container_->SetInlineStyleProperty(CSSPropertyID::kTop, current_top_,
                                         CSSPrimitiveValue::UnitType::kPixels);
}

void VTTRegion::AppendVTTCueBox(VTTCueBox* display_box) {
  DCHECK(cue_container_);

  if (cue_container_->contains(display_box))
    return;

  cue_container_->AppendChild(display_box);
  DisplayLastVTTCueBox();
}

void VTTRegion::DisplayLastVTTCueBox() {
  DVLOG(VTT_LOG_LEVEL) << "displayLastVTTCueBox";
  DCHECK(cue_container_);

  // FIXME: This should not be causing recalc styles in a loop to set the "top"
  // css property to move elements. We should just scroll the text track cues on
  // the compositor with an animation.

  if (scroll_timer_.IsActive())
    return;

  // If it's a scrolling region, add the scrolling class.
  if (IsScrollingRegion())
    cue_container_->classList().Add(TextTrackCueContainerScrollingClass());

  double region_bottom =
      region_display_tree_->getBoundingClientRect()->bottom();

  // Find first cue that is not entirely displayed and scroll it upwards.
  for (Element& child : ElementTraversal::ChildrenOf(*cue_container_)) {
    DOMRect* client_rect = child.getBoundingClientRect();
    double child_bottom = client_rect->bottom();

    if (region_bottom >= child_bottom)
      continue;

    current_top_ -=
        std::min(client_rect->height(), child_bottom - region_bottom);
    cue_container_->SetInlineStyleProperty(
        CSSPropertyID::kTop, current_top_,
        CSSPrimitiveValue::UnitType::kPixels);

    StartTimer();
    break;
  }
}

void VTTRegion::PrepareRegionDisplayTree() {
  DCHECK(region_display_tree_);

  // 7.2 Prepare region CSS boxes

  // FIXME: Change the code below to use viewport units when
  // http://crbug/244618 is fixed.

  // Let regionWidth be the text track region width.
  // Let width be 'regionWidth vw' ('vw' is a CSS unit)
  region_display_tree_->SetInlineStyleProperty(
      CSSPropertyID::kWidth, width_, CSSPrimitiveValue::UnitType::kPercentage);

  // Let lineHeight be '0.0533vh' ('vh' is a CSS unit) and regionHeight be
  // the text track region height. Let height be 'lineHeight' multiplied
  // by regionHeight.
  double height = kLineHeight * lines_;
  region_display_tree_->SetInlineStyleProperty(
      CSSPropertyID::kHeight, height,
      CSSPrimitiveValue::UnitType::kViewportHeight);

  // Let viewportAnchorX be the x dimension of the text track region viewport
  // anchor and regionAnchorX be the x dimension of the text track region
  // anchor. Let leftOffset be regionAnchorX multiplied by width divided by
  // 100.0. Let left be leftOffset subtracted from 'viewportAnchorX vw'.
  double left_offset = region_anchor_.X() * width_ / 100;
  region_display_tree_->SetInlineStyleProperty(
      CSSPropertyID::kLeft, viewport_anchor_.X() - left_offset,
      CSSPrimitiveValue::UnitType::kPercentage);

  // Let viewportAnchorY be the y dimension of the text track region viewport
  // anchor and regionAnchorY be the y dimension of the text track region
  // anchor. Let topOffset be regionAnchorY multiplied by height divided by
  // 100.0. Let top be topOffset subtracted from 'viewportAnchorY vh'.
  double top_offset = region_anchor_.Y() * height / 100;
  region_display_tree_->SetInlineStyleProperty(
      CSSPropertyID::kTop, viewport_anchor_.Y() - top_offset,
      CSSPrimitiveValue::UnitType::kPercentage);

  // The cue container is used to wrap the cues and it is the object which is
  // gradually scrolled out as multiple cues are appended to the region.
  cue_container_ =
      MakeGarbageCollected<HTMLDivElement>(region_display_tree_->GetDocument());
  cue_container_->SetInlineStyleProperty(CSSPropertyID::kTop, 0.0,
                                         CSSPrimitiveValue::UnitType::kPixels);

  cue_container_->SetShadowPseudoId(
      AtomicString("-webkit-media-text-track-region-container"));
  region_display_tree_->AppendChild(cue_container_);

  // 7.5 Every WebVTT region object is initialised with the following CSS
  region_display_tree_->SetShadowPseudoId(
      AtomicString("-webkit-media-text-track-region"));
}

void VTTRegion::StartTimer() {
  DVLOG(VTT_LOG_LEVEL) << "startTimer";

  if (scroll_timer_.IsActive())
    return;

  base::TimeDelta duration =
      IsScrollingRegion() ? kScrollTime : base::TimeDelta();
  scroll_timer_.StartOneShot(duration, FROM_HERE);
}

void VTTRegion::StopTimer() {
  DVLOG(VTT_LOG_LEVEL) << "stopTimer";
  scroll_timer_.Stop();
}

void VTTRegion::ScrollTimerFired(TimerBase*) {
  DVLOG(VTT_LOG_LEVEL) << "scrollTimerFired";

  StopTimer();
  DisplayLastVTTCueBox();
}

void VTTRegion::Trace(Visitor* visitor) {
  visitor->Trace(cue_container_);
  visitor->Trace(region_display_tree_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
