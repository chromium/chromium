/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_auto_keyword.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue_list.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_parser.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_region.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_scanner.h"
#include "third_party/blink/renderer/core/layout/layout_vtt_cue.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static const CSSValueID kDisplayWritingModeMap[] = {CSSValueID::kHorizontalTb,
                                                    CSSValueID::kVerticalRl,
                                                    CSSValueID::kVerticalLr};
static_assert(base::size(kDisplayWritingModeMap) ==
                  VTTCue::kNumberOfWritingDirections,
              "displayWritingModeMap should have the same number of elements "
              "as VTTCue::NumberOfWritingDirections");

static const CSSValueID kDisplayAlignmentMap[] = {
    CSSValueID::kStart, CSSValueID::kCenter, CSSValueID::kEnd,
    CSSValueID::kLeft, CSSValueID::kRight};
static_assert(base::size(kDisplayAlignmentMap) == VTTCue::kNumberOfAlignments,
              "displayAlignmentMap should have the same number of elements as "
              "VTTCue::NumberOfAlignments");

static const String& AutoKeyword() {
  DEFINE_STATIC_LOCAL(const String, auto_string, ("auto"));
  return auto_string;
}

static const String& StartKeyword() {
  DEFINE_STATIC_LOCAL(const String, start, ("start"));
  return start;
}

static const String& CenterKeyword() {
  DEFINE_STATIC_LOCAL(const String, center, ("center"));
  return center;
}

static const String& EndKeyword() {
  DEFINE_STATIC_LOCAL(const String, end, ("end"));
  return end;
}

static const String& LeftKeyword() {
  DEFINE_STATIC_LOCAL(const String, left, ("left"));
  return left;
}

static const String& RightKeyword() {
  DEFINE_STATIC_LOCAL(const String, right, ("right"));
  return right;
}

static const String& HorizontalKeyword() {
  return g_empty_string;
}

static const String& VerticalGrowingLeftKeyword() {
  DEFINE_STATIC_LOCAL(const String, verticalrl, ("rl"));
  return verticalrl;
}

static const String& VerticalGrowingRightKeyword() {
  DEFINE_STATIC_LOCAL(const String, verticallr, ("lr"));
  return verticallr;
}

static bool IsInvalidPercentage(double value) {
  DCHECK(std::isfinite(value));
  return value < 0 || value > 100;
}

static bool IsInvalidPercentage(double value, ExceptionState& exception_state) {
  if (IsInvalidPercentage(value)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange<double>(
            "value", value, 0, ExceptionMessages::kInclusiveBound, 100,
            ExceptionMessages::kInclusiveBound));
    return true;
  }
  return false;
}

VTTCueBox::VTTCueBox(Document& document)
    : HTMLDivElement(document),
      snap_to_lines_position_(std::numeric_limits<float>::quiet_NaN()) {
  SetShadowPseudoId(AtomicString("-webkit-media-text-track-display"));
}

VTTCueBackgroundBox::VTTCueBackgroundBox(Document& document)
    : HTMLDivElement(document) {
  SetShadowPseudoId(TextTrackCue::CueShadowPseudoId());
}

void VTTCueBackgroundBox::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_);
  HTMLDivElement::Trace(visitor);
}

void VTTCueBackgroundBox::SetTrack(TextTrack* track) {
  track_ = track;
}

void VTTCueBox::ApplyCSSProperties(
    const VTTDisplayParameters& display_parameters) {
  // http://dev.w3.org/html5/webvtt/#applying-css-properties-to-webvtt-node-objects

  // Initialize the (root) list of WebVTT Node Objects with the following CSS
  // settings:

  // the 'position' property must be set to 'absolute'
  SetInlineStyleProperty(CSSPropertyID::kPosition, CSSValueID::kAbsolute);

  //  the 'unicode-bidi' property must be set to 'plaintext'
  SetInlineStyleProperty(CSSPropertyID::kUnicodeBidi,
                         CSSValueID::kWebkitPlaintext);

  // the 'direction' property must be set to direction
  SetInlineStyleProperty(CSSPropertyID::kDirection,
                         display_parameters.direction);

  // the 'writing-mode' property must be set to writing-mode
  SetInlineStyleProperty(CSSPropertyID::kWebkitWritingMode,
                         display_parameters.writing_mode);

  const FloatPoint& position = display_parameters.position;

  // the 'top' property must be set to top,
  SetInlineStyleProperty(CSSPropertyID::kTop, position.Y(),
                         CSSPrimitiveValue::UnitType::kPercentage);

  // the 'left' property must be set to left
  SetInlineStyleProperty(CSSPropertyID::kLeft, position.X(),
                         CSSPrimitiveValue::UnitType::kPercentage);

  // the 'width' property must be set to width, and the 'height' property  must
  // be set to height
  if (display_parameters.writing_mode == CSSValueID::kHorizontalTb) {
    SetInlineStyleProperty(CSSPropertyID::kWidth, display_parameters.size,
                           CSSPrimitiveValue::UnitType::kPercentage);
    SetInlineStyleProperty(CSSPropertyID::kHeight, CSSValueID::kAuto);
  } else {
    SetInlineStyleProperty(CSSPropertyID::kWidth, CSSValueID::kAuto);
    SetInlineStyleProperty(CSSPropertyID::kHeight, display_parameters.size,
                           CSSPrimitiveValue::UnitType::kPercentage);
  }

  // The 'text-align' property on the (root) List of WebVTT Node Objects must
  // be set to the value in the second cell of the row of the table below
  // whose first cell is the value of the corresponding cue's WebVTT cue
  // text alignment:
  SetInlineStyleProperty(CSSPropertyID::kTextAlign,
                         display_parameters.text_align);

  // TODO(foolip): The position adjustment for non-snap-to-lines cues has
  // been removed from the spec:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=19178
  if (std::isnan(display_parameters.snap_to_lines_position)) {
    // 10.13.1 Set up x and y:
    // Note: x and y are set through the CSS left and top above.
    // 10.13.2 Position the boxes in boxes such that the point x% along the
    // width of the bounding box of the boxes in boxes is x% of the way
    // across the width of the video's rendering area, and the point y%
    // along the height of the bounding box of the boxes in boxes is y%
    // of the way across the height of the video's rendering area, while
    // maintaining the relative positions of the boxes in boxes to each
    // other.
    SetInlineStyleProperty(CSSPropertyID::kTransform,
                           String::Format("translate(-%.2f%%, -%.2f%%)",
                                          position.X(), position.Y()));
    SetInlineStyleProperty(CSSPropertyID::kWhiteSpace, CSSValueID::kPre);
  }

  // The snap-to-lines position is propagated to LayoutVTTCue.
  snap_to_lines_position_ = display_parameters.snap_to_lines_position;
}

LayoutObject* VTTCueBox::CreateLayoutObject(const ComputedStyle& style,
                                            LegacyLayout legacy) {
  // If WebVTT Regions are used, the regular WebVTT layout algorithm is no
  // longer necessary, since cues having the region parameter set do not have
  // any positioning parameters. Also, in this case, the regions themselves
  // have positioning information.
  if (style.GetPosition() == EPosition::kRelative)
    return HTMLDivElement::CreateLayoutObject(style, legacy);

  return new LayoutVTTCue(this, snap_to_lines_position_);
}

VTTCue::VTTCue(Document& document,
               double start_time,
               double end_time,
               const String& text)
    : TextTrackCue(start_time, end_time),
      text_(text),
      line_position_(std::numeric_limits<double>::quiet_NaN()),
      text_position_(std::numeric_limits<double>::quiet_NaN()),
      cue_size_(100),
      writing_direction_(kHorizontal),
      cue_alignment_(kCenter),
      vtt_node_tree_(nullptr),
      cue_background_box_(MakeGarbageCollected<VTTCueBackgroundBox>(document)),
      snap_to_lines_(true),
      display_tree_should_change_(true) {
  UseCounter::Count(document, WebFeature::kVTTCue);
}

VTTCue::~VTTCue() = default;

#ifndef NDEBUG
String VTTCue::ToString() const {
  return String::Format("%p id=%s interval=%f-->%f cue=%s)", this,
                        id().Utf8().c_str(), startTime(), endTime(),
                        text().Utf8().c_str());
}
#endif

void VTTCue::CueDidChange(CueMutationAffectsOrder affects_order) {
  TextTrackCue::CueDidChange(affects_order);
  display_tree_should_change_ = true;
}

const String& VTTCue::vertical() const {
  switch (writing_direction_) {
    case kHorizontal:
      return HorizontalKeyword();
    case kVerticalGrowingLeft:
      return VerticalGrowingLeftKeyword();
    case kVerticalGrowingRight:
      return VerticalGrowingRightKeyword();
    default:
      NOTREACHED();
      return g_empty_string;
  }
}

void VTTCue::setVertical(const String& value) {
  WritingDirection direction = writing_direction_;
  if (value == HorizontalKeyword())
    direction = kHorizontal;
  else if (value == VerticalGrowingLeftKeyword())
    direction = kVerticalGrowingLeft;
  else if (value == VerticalGrowingRightKeyword())
    direction = kVerticalGrowingRight;
  else
    NOTREACHED();

  if (direction == writing_direction_)
    return;

  CueWillChange();
  writing_direction_ = direction;
  CueDidChange();
}

void VTTCue::setSnapToLines(bool value) {
  if (snap_to_lines_ == value)
    return;

  CueWillChange();
  snap_to_lines_ = value;
  CueDidChange();
}

bool VTTCue::LineIsAuto() const {
  return std::isnan(line_position_);
}

void VTTCue::line(DoubleOrAutoKeyword& result) const {
  if (LineIsAuto())
    result.SetAutoKeyword(AutoKeyword());
  else
    result.SetDouble(line_position_);
}

void VTTCue::setLine(const DoubleOrAutoKeyword& position) {
  // http://dev.w3.org/html5/webvtt/#dfn-vttcue-line
  // On setting, the WebVTT cue line must be set to the new value; if the new
  // value is the string "auto", then it must be interpreted as the special
  // value auto.  ("auto" is translated to NaN.)
  double line_position;
  if (position.IsAutoKeyword()) {
    if (LineIsAuto())
      return;
    line_position = std::numeric_limits<double>::quiet_NaN();
  } else {
    DCHECK(position.IsDouble());
    line_position = position.GetAsDouble();
    if (line_position_ == line_position)
      return;
  }
  CueWillChange();
  line_position_ = line_position;
  CueDidChange();
}

bool VTTCue::TextPositionIsAuto() const {
  return std::isnan(text_position_);
}

void VTTCue::position(DoubleOrAutoKeyword& result) const {
  if (TextPositionIsAuto())
    result.SetAutoKeyword(AutoKeyword());
  else
    result.SetDouble(text_position_);
}

void VTTCue::setPosition(const DoubleOrAutoKeyword& position,
                         ExceptionState& exception_state) {
  // http://dev.w3.org/html5/webvtt/#dfn-vttcue-position
  // On setting, if the new value is negative or greater than 100, then an
  // IndexSizeError exception must be thrown. Otherwise, the WebVTT cue
  // position must be set to the new value; if the new value is the string
  // "auto", then it must be interpreted as the special value auto.
  double text_position;
  if (position.IsAutoKeyword()) {
    if (TextPositionIsAuto())
      return;
    text_position = std::numeric_limits<double>::quiet_NaN();
  } else {
    DCHECK(position.IsDouble());
    if (IsInvalidPercentage(position.GetAsDouble(), exception_state))
      return;
    text_position = position.GetAsDouble();
    if (text_position_ == text_position)
      return;
  }

  CueWillChange();
  text_position_ = text_position;
  CueDidChange();
}

void VTTCue::setSize(double size, ExceptionState& exception_state) {
  // http://dev.w3.org/html5/webvtt/#dfn-vttcue-size
  // On setting, if the new value is negative or greater than 100, then throw
  // an IndexSizeError exception.
  if (IsInvalidPercentage(size, exception_state))
    return;

  // Otherwise, set the WebVTT cue size to the new value.
  if (cue_size_ == size)
    return;

  CueWillChange();
  cue_size_ = size;
  CueDidChange();
}

const String& VTTCue::align() const {
  switch (cue_alignment_) {
    case kStart:
      return StartKeyword();
    case kCenter:
      return CenterKeyword();
    case kEnd:
      return EndKeyword();
    case kLeft:
      return LeftKeyword();
    case kRight:
      return RightKeyword();
    default:
      NOTREACHED();
      return g_empty_string;
  }
}

void VTTCue::setAlign(const String& value) {
  CueAlignment alignment = cue_alignment_;
  if (value == StartKeyword())
    alignment = kStart;
  else if (value == CenterKeyword())
    alignment = kCenter;
  else if (value == EndKeyword())
    alignment = kEnd;
  else if (value == LeftKeyword())
    alignment = kLeft;
  else if (value == RightKeyword())
    alignment = kRight;
  else
    NOTREACHED();

  if (alignment == cue_alignment_)
    return;

  CueWillChange();
  cue_alignment_ = alignment;
  CueDidChange();
}

void VTTCue::setText(const String& text) {
  if (text_ == text)
    return;

  CueWillChange();
  // Clear the document fragment but don't bother to create it again just yet as
  // we can do that when it is requested.
  vtt_node_tree_ = nullptr;
  text_ = text;
  CueDidChange();
}

void VTTCue::CreateVTTNodeTree() {
  if (!vtt_node_tree_) {
    vtt_node_tree_ = VTTParser::CreateDocumentFragmentFromCueText(
        GetDocument(), text_, this->track());
    cue_background_box_->SetTrack(this->track());
  }
}

void VTTCue::CopyVTTNodeToDOMTree(ContainerNode* vtt_node,
                                  ContainerNode* parent) {
  for (Node* node = vtt_node->firstChild(); node; node = node->nextSibling()) {
    Node* cloned_node;
    if (auto* vtt_element = DynamicTo<VTTElement>(node))
      cloned_node = vtt_element->CreateEquivalentHTMLElement(GetDocument());
    else
      cloned_node = node->cloneNode(false);
    parent->AppendChild(cloned_node);
    auto* container_node = DynamicTo<ContainerNode>(node);
    if (container_node)
      CopyVTTNodeToDOMTree(container_node, To<ContainerNode>(cloned_node));
  }
}

DocumentFragment* VTTCue::getCueAsHTML() {
  CreateVTTNodeTree();
  DocumentFragment* cloned_fragment = DocumentFragment::Create(GetDocument());
  CopyVTTNodeToDOMTree(vtt_node_tree_.Get(), cloned_fragment);
  return cloned_fragment;
}

void VTTCue::setRegion(VTTRegion* region) {
  if (region_ == region)
    return;
  CueWillChange();
  region_ = region;
  CueDidChange();
}

double VTTCue::CalculateComputedLinePosition() const {
  // http://dev.w3.org/html5/webvtt/#dfn-cue-computed-line
  // A WebVTT cue has a computed line whose value is that returned by the
  // following algorithm, which is defined in terms of the other aspects of
  // the cue:

  // 1. If the line is numeric, the WebVTT cue snap-to-lines flag of the
  //    WebVTT cue is not set, and the line is negative or greater than 100,
  //    then return 100 and abort these steps.
  if (!LineIsAuto() && !snap_to_lines_ && IsInvalidPercentage(line_position_))
    return 100;

  // 2. If the line is numeric, return the value of the WebVTT cue line and
  //    abort these steps. (Either the WebVTT cue snap-to-lines flag is set,
  //    so any value, not just those in the range 0..100, is valid, or the
  //    value is in the range 0..100 and is thus valid regardless of the
  //    value of that flag.)
  if (!LineIsAuto())
    return line_position_;

  // 3. If the WebVTT cue snap-to-lines flag of the WebVTT cue is not set,
  //    return the value 100 and abort these steps. (The WebVTT cue line is
  //    the special value auto.)
  if (!snap_to_lines_)
    return 100;

  // 4. Let cue be the WebVTT cue.
  // 5. If cue is not in a list of cues of a text track, or if that text
  //    track is not in the list of text tracks of a media element, return -1
  //    and abort these steps.
  if (!track())
    return -1;

  // 6. Let track be the text track whose list of cues the cue is in.
  // 7. Let n be the number of text tracks whose text track mode is showing
  //    and that are in the media element's list of text tracks before track.
  int n = track()->TrackIndexRelativeToRenderedTracks();

  // 8. Increment n by one. / 9. Negate n. / 10. Return n.
  n++;
  n = -n;
  return n;
}

class VTTTextRunIterator : public TextRunIterator {
 public:
  VTTTextRunIterator() = default;
  VTTTextRunIterator(const TextRun* text_run, unsigned offset)
      : TextRunIterator(text_run, offset) {}

  bool AtParagraphSeparator() const {
    // Within a cue, paragraph boundaries are only denoted by Type B characters,
    // such as U+000A LINE FEED (LF), U+0085 NEXT LINE (NEL),
    // and U+2029 PARAGRAPH SEPARATOR.
    return WTF::unicode::Category(Current()) &
           WTF::unicode::kSeparator_Paragraph;
  }
};

// Almost the same as determineDirectionality in core/html/html_element.cc, but
// that one uses a "plain" TextRunIterator (which only checks for '\n').
static TextDirection DetermineDirectionality(const String& value,
                                             bool& has_strong_directionality) {
  TextRun run(value);
  BidiResolver<VTTTextRunIterator, BidiCharacterRun> bidi_resolver;
  bidi_resolver.SetStatus(BidiStatus(TextDirection::kLtr, false));
  bidi_resolver.SetPositionIgnoringNestedIsolates(VTTTextRunIterator(&run, 0));
  return bidi_resolver.DetermineDirectionality(&has_strong_directionality);
}

static CSSValueID DetermineTextDirection(DocumentFragment* vtt_root) {
  DCHECK(vtt_root);

  // Apply the Unicode Bidirectional Algorithm's Paragraph Level steps to the
  // concatenation of the values of each WebVTT Text Object in nodes, in a
  // pre-order, depth-first traversal, excluding WebVTT Ruby Text Objects and
  // their descendants.
  TextDirection text_direction = TextDirection::kLtr;
  Node* node = NodeTraversal::Next(*vtt_root);
  while (node) {
    DCHECK(node->IsDescendantOf(vtt_root));

    if (node->IsTextNode()) {
      bool has_strong_directionality;
      text_direction =
          DetermineDirectionality(node->nodeValue(), has_strong_directionality);
      if (has_strong_directionality)
        break;
    } else if (auto* vtt_element = DynamicTo<VTTElement>(node)) {
      if (vtt_element->WebVTTNodeType() == kVTTNodeTypeRubyText) {
        node = NodeTraversal::NextSkippingChildren(*node);
        continue;
      }
    }

    node = NodeTraversal::Next(*node);
  }
  return IsLtr(text_direction) ? CSSValueID::kLtr : CSSValueID::kRtl;
}

double VTTCue::CalculateComputedTextPosition() const {
  // http://dev.w3.org/html5/webvtt/#dfn-cue-computed-position

  // 1. If the position is numeric, then return the value of the position and
  // abort these steps. (Otherwise, the position is the special value auto.)
  if (!TextPositionIsAuto())
    return text_position_;

  switch (cue_alignment_) {
    // 2. If the cue text alignment is start or left, return 0 and abort these
    // steps.
    case kStart:
    case kLeft:
      return 0;
    // 3. If the cue text alignment is end or right, return 100 and abort these
    // steps.
    case kEnd:
    case kRight:
      return 100;
    // 4. If the cue text alignment is center, return 50 and abort these steps.
    case kCenter:
      return 50;
    default:
      NOTREACHED();
      return 0;
  }
}

VTTCue::CueAlignment VTTCue::CalculateComputedCueAlignment() const {
  switch (cue_alignment_) {
    case VTTCue::kLeft:
      return VTTCue::kStart;
    case VTTCue::kRight:
      return VTTCue::kEnd;
    default:
      return cue_alignment_;
  }
}

VTTDisplayParameters::VTTDisplayParameters()
    : size(std::numeric_limits<float>::quiet_NaN()),
      direction(CSSValueID::kNone),
      text_align(CSSValueID::kNone),
      writing_mode(CSSValueID::kNone),
      snap_to_lines_position(std::numeric_limits<float>::quiet_NaN()) {}

VTTDisplayParameters VTTCue::CalculateDisplayParameters() const {
  // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings

  VTTDisplayParameters display_parameters;

  // Steps 1 and 2.
  display_parameters.direction = DetermineTextDirection(vtt_node_tree_.Get());

  if (display_parameters.direction == CSSValueID::kRtl)
    UseCounter::Count(GetDocument(), WebFeature::kVTTCueRenderRtl);

  // Note: The 'text-align' property is also determined here so that
  // VTTCueBox::applyCSSProperties need not have access to a VTTCue.
  display_parameters.text_align = kDisplayAlignmentMap[GetCueAlignment()];

  // 3. If the cue writing direction is horizontal, then let block-flow be
  // 'tb'. Otherwise, if the cue writing direction is vertical growing left,
  // then let block-flow be 'lr'. Otherwise, the cue writing direction is
  // vertical growing right; let block-flow be 'rl'.
  display_parameters.writing_mode = kDisplayWritingModeMap[writing_direction_];

  // Resolve the cue alignment to one of the values {start, end, center}.
  CueAlignment computed_cue_alignment = CalculateComputedCueAlignment();

  // 4. Determine the value of maximum size for cue as per the appropriate
  // rules from the following list:
  double computed_text_position = CalculateComputedTextPosition();
  double maximum_size = computed_text_position;
  if (computed_cue_alignment == kStart) {
    maximum_size = 100 - computed_text_position;
  } else if (computed_cue_alignment == kEnd) {
    maximum_size = computed_text_position;
  } else if (computed_cue_alignment == kCenter) {
    maximum_size = computed_text_position <= 50
                       ? computed_text_position
                       : (100 - computed_text_position);
    maximum_size = maximum_size * 2;
  } else {
    NOTREACHED();
  }

  // 5. If the cue size is less than maximum size, then let size
  // be cue size. Otherwise, let size be maximum size.
  display_parameters.size = std::min(cue_size_, maximum_size);

  // 6. If the cue writing direction is horizontal, then let width
  // be 'size vw' and height be 'auto'. Otherwise, let width be 'auto' and
  // height be 'size vh'. (These are CSS values used by the next section to
  // set CSS properties for the rendering; 'vw' and 'vh' are CSS units.)
  // (Emulated in VTTCueBox::applyCSSProperties.)

  // 7. Determine the value of x-position or y-position for cue as per the
  // appropriate rules from the following list:
  if (writing_direction_ == kHorizontal) {
    switch (computed_cue_alignment) {
      case kStart:
        display_parameters.position.SetX(computed_text_position);
        break;
      case kEnd:
        display_parameters.position.SetX(computed_text_position -
                                         display_parameters.size);
        break;
      case kCenter:
        display_parameters.position.SetX(computed_text_position -
                                         display_parameters.size / 2);
        break;
      default:
        NOTREACHED();
    }
  } else {
    // Cases for writing_direction_ being kVerticalGrowing{Left|Right}
    switch (computed_cue_alignment) {
      case kStart:
        display_parameters.position.SetY(computed_text_position);
        break;
      case kEnd:
        display_parameters.position.SetY(computed_text_position -
                                         display_parameters.size);
        break;
      case kCenter:
        display_parameters.position.SetY(computed_text_position -
                                         display_parameters.size / 2);
        break;
      default:
        NOTREACHED();
    }
  }

  // A cue has a computed line whose value is defined in terms of
  // the other aspects of the cue.
  double computed_line_position = CalculateComputedLinePosition();

  // 8. Determine the value of whichever of x-position or y-position is not
  // yet calculated for cue as per the appropriate rules from the following
  // list:
  if (!snap_to_lines_) {
    if (writing_direction_ == kHorizontal)
      display_parameters.position.SetY(computed_line_position);
    else
      display_parameters.position.SetX(computed_line_position);
  } else {
    if (writing_direction_ == kHorizontal)
      display_parameters.position.SetY(0);
    else
      display_parameters.position.SetX(0);
  }

  // Step 9 not implemented (margin == 0).

  // The snap-to-lines position is propagated to LayoutVTTCue.
  display_parameters.snap_to_lines_position =
      snap_to_lines_ ? computed_line_position
                     : std::numeric_limits<float>::quiet_NaN();

  DCHECK(std::isfinite(display_parameters.size));
  DCHECK_NE(display_parameters.direction, CSSValueID::kNone);
  DCHECK_NE(display_parameters.writing_mode, CSSValueID::kNone);
  return display_parameters;
}

void VTTCue::UpdatePastAndFutureNodes(double movie_time) {
  DEFINE_STATIC_LOCAL(const String, timestamp_tag, ("timestamp"));

  DCHECK(IsActive());

  // An active cue may still not have a display tree, e.g. if its track is
  // hidden or if the track belongs to an audio element.
  if (!display_tree_)
    return;

  // FIXME: Per spec it's possible for neither :past nor :future to match, but
  // as implemented here and in SelectorChecker they are simply each others
  // negations. For a cue with no internal timestamps, :past will match but
  // should not per spec. :future is correct, however. See the spec bug to
  // determine what the correct behavior should be:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28237

  bool is_past_node = true;
  double current_timestamp = startTime();
  if (current_timestamp > movie_time)
    is_past_node = false;

  for (Node& child : NodeTraversal::DescendantsOf(*display_tree_)) {
    if (child.nodeName() == timestamp_tag) {
      bool check =
          VTTParser::CollectTimeStamp(child.nodeValue(), current_timestamp);
      DCHECK(check);

      if (current_timestamp > movie_time)
        is_past_node = false;
    }

    if (auto* child_vtt_element = DynamicTo<VTTElement>(child)) {
      child_vtt_element->SetIsPastNode(is_past_node);
      // Make an elemenet id match a cue id for style matching purposes.
      if (!id().IsEmpty())
        To<Element>(child).SetIdAttribute(id());
    }
  }
}

VTTCueBox* VTTCue::GetDisplayTree() {
  DCHECK(track() && track()->IsRendered() && IsActive());

  if (!display_tree_) {
    display_tree_ = MakeGarbageCollected<VTTCueBox>(GetDocument());
    display_tree_->AppendChild(cue_background_box_);
  }

  DCHECK_EQ(display_tree_->firstChild(), cue_background_box_);

  if (!display_tree_should_change_)
    return display_tree_;

  CreateVTTNodeTree();

  cue_background_box_->RemoveChildren();
  cue_background_box_->CloneChildNodesFrom(*vtt_node_tree_);

  if (!region()) {
    VTTDisplayParameters display_parameters = CalculateDisplayParameters();
    display_tree_->ApplyCSSProperties(display_parameters);
  } else {
    display_tree_->SetInlineStyleProperty(CSSPropertyID::kPosition,
                                          CSSValueID::kRelative);
  }

  display_tree_should_change_ = false;

  return display_tree_;
}

void VTTCue::RemoveDisplayTree(RemovalNotification removal_notification) {
  if (!display_tree_)
    return;
  if (removal_notification == kNotifyRegion) {
    // The region needs to be informed about the cue removal.
    if (region())
      region()->WillRemoveVTTCueBox(display_tree_);
  }
  display_tree_->remove(ASSERT_NO_EXCEPTION);
}

void VTTCue::UpdateDisplay(HTMLDivElement& container) {
  DCHECK(track() && track()->IsRendered() && IsActive());

  UseCounter::Count(GetDocument(), WebFeature::kVTTCueRender);

  if (writing_direction_ != kHorizontal)
    UseCounter::Count(GetDocument(), WebFeature::kVTTCueRenderVertical);

  if (!snap_to_lines_)
    UseCounter::Count(GetDocument(), WebFeature::kVTTCueRenderSnapToLinesFalse);

  if (!LineIsAuto())
    UseCounter::Count(GetDocument(), WebFeature::kVTTCueRenderLineNotAuto);

  if (TextPositionIsAuto())
    UseCounter::Count(GetDocument(), WebFeature::kVTTCueRenderPositionNot50);

  if (cue_size_ != 100)
    UseCounter::Count(GetDocument(), WebFeature::kVTTCueRenderSizeNot100);

  if (cue_alignment_ != kCenter)
    UseCounter::Count(GetDocument(), WebFeature::kVTTCueRenderAlignNotCenter);

  VTTCueBox* display_box = GetDisplayTree();
  if (!region()) {
    if (display_box->HasChildren() && !container.contains(display_box)) {
      // Note: the display tree of a cue is removed when the active flag of the
      // cue is unset.
      container.AppendChild(display_box);
    }
  } else {
    HTMLDivElement* region_node = region()->GetDisplayTree(GetDocument());

    // Append the region to the viewport, if it was not already.
    if (!container.contains(region_node))
      container.AppendChild(region_node);

    region()->AppendVTTCueBox(display_box);
  }
}

VTTCue::CueSetting VTTCue::SettingName(VTTScanner& input) const {
  CueSetting parsed_setting = kNone;
  if (input.Scan("vertical"))
    parsed_setting = kVertical;
  else if (input.Scan("line"))
    parsed_setting = kLine;
  else if (input.Scan("position"))
    parsed_setting = kPosition;
  else if (input.Scan("size"))
    parsed_setting = kSize;
  else if (input.Scan("align"))
    parsed_setting = kAlign;
  else if (RuntimeEnabledFeatures::WebVTTRegionsEnabled() &&
           input.Scan("region"))
    parsed_setting = kRegionId;
  // Verify that a ':' follows.
  if (parsed_setting != kNone && input.Scan(':'))
    return parsed_setting;
  return kNone;
}

static bool ScanPercentage(VTTScanner& input, double& number) {
  // http://dev.w3.org/html5/webvtt/#dfn-parse-a-percentage-string

  // 1. Let input be the string being parsed.
  // 2. If input contains any characters other than U+0025 PERCENT SIGN
  //    characters (%), U+002E DOT characters (.) and ASCII digits, then
  //    fail.
  // 3. If input does not contain at least one ASCII digit, then fail.
  // 4. If input contains more than one U+002E DOT character (.), then fail.
  // 5. If any character in input other than the last character is a U+0025
  //    PERCENT SIGN character (%), then fail.
  // 6. If the last character in input is not a U+0025 PERCENT SIGN character
  //    (%), then fail.
  // 7. Ignoring the trailing percent sign, interpret input as a real
  //    number. Let that number be the percentage.
  // 8. If percentage is outside the range 0..100, then fail.
  // 9. Return percentage.
  return input.ScanPercentage(number) && !IsInvalidPercentage(number);
}

void VTTCue::ParseSettings(const VTTRegionMap* region_map,
                           const String& input_string) {
  VTTScanner input(input_string);

  while (!input.IsAtEnd()) {
    // The WebVTT cue settings part of a WebVTT cue consists of zero or more of
    // the following components, in any order, separated from each other by one
    // or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION (tab)
    // characters.
    input.SkipWhile<VTTParser::IsValidSettingDelimiter>();

    if (input.IsAtEnd())
      break;

    // When the user agent is to parse the WebVTT settings given by a string
    // input for a text track cue cue,
    // the user agent must run the following steps:
    // 1. Let settings be the result of splitting input on spaces.
    // 2. For each token setting in the list settings, run the following
    //    substeps:
    //    1. If setting does not contain a U+003A COLON character (:), or if the
    //       first U+003A COLON character (:) in setting is either the first or
    //       last character of setting, then jump to the step labeled next
    //       setting.
    //    2. Let name be the leading substring of setting up to and excluding
    //       the first U+003A COLON character (:) in that string.
    CueSetting name = SettingName(input);

    // 3. Let value be the trailing substring of setting starting from the
    //    character immediately after the first U+003A COLON character (:) in
    //    that string.
    VTTScanner::Run value_run =
        input.CollectUntil<VTTParser::IsValidSettingDelimiter>();

    // 4. Run the appropriate substeps that apply for the value of name, as
    //    follows:
    switch (name) {
      case kVertical: {
        // If name is a case-sensitive match for "vertical"
        // 1. If value is a case-sensitive match for the string "rl", then
        //    let cue's WebVTT cue writing direction be vertical
        //    growing left.
        if (input.ScanRun(value_run, VerticalGrowingLeftKeyword()))
          writing_direction_ = kVerticalGrowingLeft;

        // 2. Otherwise, if value is a case-sensitive match for the string
        //    "lr", then let cue's WebVTT cue writing direction be
        //    vertical growing right.
        else if (input.ScanRun(value_run, VerticalGrowingRightKeyword()))
          writing_direction_ = kVerticalGrowingRight;
        break;
      }
      case kLine: {
        // If name is a case-sensitive match for "line"
        // Steps 1 - 2 skipped.
        double number;
        // 3. If linepos does not contain at least one ASCII digit, then
        //    jump to the step labeled next setting.
        // 4. If the last character in linepos is a U+0025 PERCENT SIGN
        //    character (%)
        //
        //    If parse a percentage string from linepos doesn't fail, let
        //    number be the returned percentage, otherwise jump to the step
        //    labeled next setting.
        bool is_percentage = input.ScanPercentage(number);
        if (is_percentage) {
          if (IsInvalidPercentage(number))
            break;
        } else {
          // Otherwise
          //
          // 1. If linepos contains any characters other than U+002D
          //    HYPHEN-MINUS characters (-), ASCII digits, and U+002E DOT
          //    character (.), then jump to the step labeled next setting.
          //
          // 2. If any character in linepos other than the first character is a
          //    U+002D HYPHEN-MINUS character (-), then jump to the step
          //    labeled next setting.
          //
          // 3. If there are more than one U+002E DOT characters (.), then jump
          //    to the step labeled next setting.
          //
          // 4. If there is a U+002E DOT character (.) and the character before
          //    or the character after is not an ASCII digit, or if the U+002E
          //    DOT character (.) is the first or the last character, then jump
          //    to the step labeled next setting.
          //
          // 5. Interpret linepos as a (potentially signed) real number, and
          //    let number be that number.
          bool is_negative = input.Scan('-');
          if (!input.ScanDouble(number))
            break;
          // Negate number if it was preceded by a hyphen-minus - unless it's
          // zero.
          if (is_negative && number)
            number = -number;
        }
        if (!input.IsAt(value_run.end()))
          break;
        // 5. Let cue's WebVTT cue line be number.
        line_position_ = number;
        // 6. If the last character in linepos is a U+0025 PERCENT SIGN
        //    character (%), then let cue's WebVTT cue snap-to-lines
        //    flag be false. Otherwise, let it be true.
        snap_to_lines_ = !is_percentage;
        // Steps 7 - 9 skipped.
        break;
      }
      case kPosition: {
        // If name is a case-sensitive match for "position".
        double number;
        // Steps 1 - 2 skipped.
        // 3. If parse a percentage string from colpos doesn't fail, let
        //    number be the returned percentage, otherwise jump to the step
        //    labeled next setting (text track cue text position's value
        //    remains the special value auto).
        if (!ScanPercentage(input, number))
          break;
        if (!input.IsAt(value_run.end()))
          break;
        // 4. Let cue's cue position be number.
        text_position_ = number;
        // Steps 5 - 7 skipped.
        break;
      }
      case kSize: {
        // If name is a case-sensitive match for "size"
        double number;
        // 1. If parse a percentage string from value doesn't fail, let
        //    number be the returned percentage, otherwise jump to the step
        //    labeled next setting.
        if (!ScanPercentage(input, number))
          break;
        if (!input.IsAt(value_run.end()))
          break;
        // 2. Let cue's WebVTT cue size be number.
        cue_size_ = number;
        break;
      }
      case kAlign: {
        // If name is a case-sensitive match for "align"
        // 1. If value is a case-sensitive match for the string "start",
        //    then let cue's WebVTT cue text alignment be start alignment.
        if (input.ScanRun(value_run, StartKeyword()))
          cue_alignment_ = kStart;

        // 2. If value is a case-sensitive match for the string "center",
        //    then let cue's WebVTT cue text alignment be center alignment.
        else if (input.ScanRun(value_run, CenterKeyword()))
          cue_alignment_ = kCenter;

        // 3. If value is a case-sensitive match for the string "end", then
        //    let cue's WebVTT cue text alignment be end alignment.
        else if (input.ScanRun(value_run, EndKeyword()))
          cue_alignment_ = kEnd;

        // 4. If value is a case-sensitive match for the string "left",
        //    then let cue's WebVTT cue text alignment be left alignment.
        else if (input.ScanRun(value_run, LeftKeyword()))
          cue_alignment_ = kLeft;

        // 5. If value is a case-sensitive match for the string "right",
        //    then let cue's WebVTT cue text alignment be right alignment.
        else if (input.ScanRun(value_run, RightKeyword()))
          cue_alignment_ = kRight;
        break;
      }
      case kRegionId:
        if (region_map)
          region_ = region_map->at(input.ExtractString(value_run));
        break;
      case kNone:
        break;
    }

    // Make sure the entire run is consumed.
    input.SkipRun(value_run);
  }
}

ExecutionContext* VTTCue::GetExecutionContext() const {
  DCHECK(cue_background_box_);
  return cue_background_box_->GetExecutionContext();
}

Document& VTTCue::GetDocument() const {
  DCHECK(cue_background_box_);
  return cue_background_box_->GetDocument();
}

void VTTCue::Trace(Visitor* visitor) {
  visitor->Trace(region_);
  visitor->Trace(vtt_node_tree_);
  visitor->Trace(cue_background_box_);
  visitor->Trace(display_tree_);
  TextTrackCue::Trace(visitor);
}

}  // namespace blink
