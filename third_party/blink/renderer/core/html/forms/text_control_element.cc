/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_selection_mode.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

Position GetNextSoftBreak(const OffsetMapping& mapping, InlineCursor& cursor) {
  while (cursor) {
    DCHECK(cursor.Current().IsLineBox()) << cursor;
    const auto* break_token = cursor.Current().GetInlineBreakToken();
    cursor.MoveToNextLine();
    // We don't need to emit a LF for the last line.
    if (!cursor)
      return Position();
    if (break_token && !break_token->IsForcedBreak())
      return mapping.GetFirstPosition(break_token->StartTextOffset());
  }
  return Position();
}

}  // namespace

TextControlElement::TextControlElement(const QualifiedName& tag_name,
                                       Document& doc)
    : HTMLFormControlElementWithState(tag_name, doc),
      last_change_was_user_edit_(false),
      cached_selection_start_(0),
      cached_selection_end_(0) {
  cached_selection_direction_ =
      doc.GetFrame() && doc.GetFrame()
                            ->GetEditor()
                            .Behavior()
                            .ShouldConsiderSelectionAsDirectional()
          ? kSelectionHasForwardDirection
          : kSelectionHasNoDirection;
}

TextControlElement::~TextControlElement() = default;

bool TextControlElement::DispatchFocusEvent(
    Element* old_focused_element,
    mojom::blink::FocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (SupportsPlaceholder())
    UpdatePlaceholderVisibility();
  HandleFocusEvent(old_focused_element, type);
  return HTMLFormControlElementWithState::DispatchFocusEvent(
      old_focused_element, type, source_capabilities);
}

void TextControlElement::DispatchBlurEvent(
    Element* new_focused_element,
    mojom::blink::FocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (SupportsPlaceholder())
    UpdatePlaceholderVisibility();
  HandleBlurEvent();
  HTMLFormControlElementWithState::DispatchBlurEvent(new_focused_element, type,
                                                     source_capabilities);
}

void TextControlElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kWebkitEditableContentChanged &&
      GetLayoutObject() && GetLayoutObject()->IsTextControl()) {
    last_change_was_user_edit_ = !GetDocument().IsRunningExecCommand();
    if (last_change_was_user_edit_) {
      SetUserHasEditedTheField();
    }

    if (IsFocused()) {
      // Updating the cache in SelectionChanged() isn't enough because
      // SelectionChanged() is not called if:
      // - Text nodes in the inner-editor is split to multiple, and
      // - The caret is on the beginning of a Text node, and its previous node
      //   is updated, or
      // - The caret is on the end of a text node, and its next node is updated.
      ComputedSelection computed_selection;
      ComputeSelection(kStart | kEnd | kDirection, computed_selection);
      CacheSelection(computed_selection.start, computed_selection.end,
                     computed_selection.direction);
    }

    SubtreeHasChanged();
    return;
  }

  HTMLFormControlElementWithState::DefaultEventHandler(event);
}

void TextControlElement::ForwardEvent(Event& event) {
  if (event.type() == event_type_names::kBlur ||
      event.type() == event_type_names::kFocus)
    return;
  if (auto* inner_editor = InnerEditorElement()) {
    inner_editor->DefaultEventHandler(event);
  }
}

String TextControlElement::StrippedPlaceholder() const {
  // According to the HTML5 specification, we need to remove CR and LF from
  // the attribute value.
  const AtomicString& attribute_value =
      FastGetAttribute(html_names::kPlaceholderAttr);
  if (!attribute_value.Contains(kNewlineCharacter) &&
      !attribute_value.Contains(kCarriageReturnCharacter))
    return attribute_value;

  StringBuilder stripped;
  unsigned length = attribute_value.length();
  stripped.ReserveCapacity(length);
  for (unsigned i = 0; i < length; ++i) {
    UChar character = attribute_value[i];
    if (character == kNewlineCharacter || character == kCarriageReturnCharacter)
      continue;
    stripped.Append(character);
  }
  return stripped.ToString();
}

bool TextControlElement::PlaceholderShouldBeVisible() const {
  return SuggestedValue().empty() && SupportsPlaceholder() &&
         FastHasAttribute(html_names::kPlaceholderAttr) &&
         IsInnerEditorValueEmpty();
}

HTMLElement* TextControlElement::PlaceholderElement() const {
  ShadowRoot* root = UserAgentShadowRoot();
  if (!root) {
    return nullptr;
  }
  if (!SupportsPlaceholder())
    return nullptr;
  auto* element = root->getElementById(shadow_element_names::kIdPlaceholder);
  CHECK(!element || IsA<HTMLElement>(element));
  return To<HTMLElement>(element);
}

void TextControlElement::UpdatePlaceholderVisibility() {
  bool place_holder_was_visible = IsPlaceholderVisible();
  HTMLElement* placeholder = PlaceholderElement();
  if (!placeholder) {
    if (RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled() &&
        !InnerEditorElement()) {
      // The place holder visibility needs to be updated as it may be used by
      // CSS selectors.
      SetPlaceholderVisibility(PlaceholderShouldBeVisible());
      return;
    }
    placeholder = UpdatePlaceholderText();
  }
  SetPlaceholderVisibility(PlaceholderShouldBeVisible());

  if (placeholder) {
    placeholder->SetInlineStyleProperty(
        CSSPropertyID::kDisplay,
        // The placeholder "element" is used to display both the placeholder
        // "value" and the suggested value. Which is why even if the placeholder
        // value is not visible, we still show the placeholder element during a
        // preview state so that the suggested value becomes visible. This
        // mechanism will change, since Autofill previews are expected to move
        // to the browser process (as per crbug.com/1474969).
        IsPlaceholderVisible() || !SuggestedValue().IsNull()
            ? CSSValueID::kBlock
            : CSSValueID::kNone,
        true);
  }

  // If there was a visibility change not caused by the suggested value, set
  // that the pseudo state changed.
  if (place_holder_was_visible != IsPlaceholderVisible() &&
      SuggestedValue().empty()) {
    PseudoStateChanged(CSSSelector::kPseudoPlaceholderShown);
  }
}

void TextControlElement::UpdatePlaceholderShadowPseudoId(
    HTMLElement& placeholder) {
  if (suggested_value_.empty()) {
    // Reset the pseudo-id for placeholders to use the appropriated style
    placeholder.SetShadowPseudoId(
        shadow_element_names::kPseudoInputPlaceholder);
  } else {
    // Set the pseudo-id for suggested values to use the appropriated style.
    placeholder.SetShadowPseudoId(
        shadow_element_names::kPseudoInternalInputSuggested);
  }
}

void TextControlElement::setSelectionStart(unsigned start) {
  setSelectionRangeForBinding(start, std::max(start, selectionEnd()),
                              selectionDirection());
}

void TextControlElement::setSelectionEnd(unsigned end) {
  setSelectionRangeForBinding(std::min(end, selectionStart()), end,
                              selectionDirection());
}

void TextControlElement::setSelectionDirection(const String& direction) {
  setSelectionRangeForBinding(selectionStart(), selectionEnd(), direction);
}

void TextControlElement::select() {
  setSelectionRangeForBinding(0, std::numeric_limits<unsigned>::max());
  // Avoid SelectionBehaviorOnFocus::Restore, which scrolls containers to show
  // the selection.
  Focus(FocusParams(SelectionBehaviorOnFocus::kNone,
                    mojom::blink::FocusType::kScript, nullptr,
                    FocusOptions::Create()));
  RestoreCachedSelection();
}

void TextControlElement::SetValueBeforeFirstUserEditIfNotSet() {
  if (!value_before_first_user_edit_.IsNull())
    return;
  String value = this->Value();
  value_before_first_user_edit_ = value.IsNull() ? g_empty_string : value;
}

void TextControlElement::CheckIfValueWasReverted(const String& value) {
  DCHECK(!value_before_first_user_edit_.IsNull())
      << "setValueBeforeFirstUserEditIfNotSet should be called beforehand.";
  String non_null_value = value.IsNull() ? g_empty_string : value;
  if (value_before_first_user_edit_ == non_null_value)
    ClearValueBeforeFirstUserEdit();
}

void TextControlElement::ClearValueBeforeFirstUserEdit() {
  value_before_first_user_edit_ = String();
}

void TextControlElement::SetFocused(bool flag,
                                    mojom::blink::FocusType focus_type) {
  HTMLFormControlElementWithState::SetFocused(flag, focus_type);

  if (!flag)
    DispatchFormControlChangeEvent();

  if (auto* inner_editor = InnerEditorElement())
    inner_editor->FocusChanged();
}

void TextControlElement::DispatchFormControlChangeEvent() {
  if (!value_before_first_user_edit_.IsNull() &&
      !EqualIgnoringNullity(value_before_first_user_edit_, Value())) {
    ClearValueBeforeFirstUserEdit();
    DispatchChangeEvent();
  } else {
    ClearValueBeforeFirstUserEdit();
  }
}

void TextControlElement::EnqueueChangeEvent() {
  if (!value_before_first_user_edit_.IsNull() &&
      !EqualIgnoringNullity(value_before_first_user_edit_, Value())) {
    Event* event = Event::CreateBubble(event_type_names::kChange);
    event->SetTarget(this);
    GetDocument().EnqueueAnimationFrameEvent(event);
  }
  ClearValueBeforeFirstUserEdit();
}

void TextControlElement::setRangeText(const String& replacement,
                                      ExceptionState& exception_state) {
  setRangeText(replacement, selectionStart(), selectionEnd(),
               V8SelectionMode(V8SelectionMode::Enum::kPreserve),
               exception_state);
}

void TextControlElement::setRangeText(const String& replacement,
                                      unsigned start,
                                      unsigned end,
                                      const V8SelectionMode& selection_mode,
                                      ExceptionState& exception_state) {
  if (start > end) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The provided start value (" + String::Number(start) +
            ") is larger than the provided end value (" + String::Number(end) +
            ").");
    return;
  }
  if (OpenShadowRoot())
    return;

  String original_text = InnerEditorValue();
  unsigned text_length = original_text.length();
  unsigned replacement_length = replacement.length();
  unsigned new_selection_start = selectionStart();
  unsigned new_selection_end = selectionEnd();

  start = std::min(start, text_length);
  end = std::min(end, text_length);

  StringBuilder text;
  text.Append(StringView(original_text, 0, start));
  text.Append(replacement);
  text.Append(StringView(original_text, end));

  SetValue(text.ToString(), TextFieldEventBehavior::kDispatchNoEvent,
           TextControlSetValueSelection::kDoNotSet);

  switch (selection_mode.AsEnum()) {
    case V8SelectionMode::Enum::kSelect:
      new_selection_start = start;
      new_selection_end = start + replacement_length;
      break;
    case V8SelectionMode::Enum::kStart:
      new_selection_start = new_selection_end = start;
      break;
    case V8SelectionMode::Enum::kEnd:
      new_selection_start = new_selection_end = start + replacement_length;
      break;
    case V8SelectionMode::Enum::kPreserve: {
      int delta = replacement_length - (end - start);

      if (new_selection_start > end)
        new_selection_start += delta;
      else if (new_selection_start > start)
        new_selection_start = start;

      if (new_selection_end > end)
        new_selection_end += delta;
      else if (new_selection_end > start)
        new_selection_end = start + replacement_length;
      break;
    }
  }

  setSelectionRangeForBinding(new_selection_start, new_selection_end);
}

void TextControlElement::setSelectionRangeForBinding(
    unsigned start,
    unsigned end,
    const String& direction_string) {
  TextFieldSelectionDirection direction = kSelectionHasNoDirection;
  if (direction_string == "forward")
    direction = kSelectionHasForwardDirection;
  else if (direction_string == "backward")
    direction = kSelectionHasBackwardDirection;
  if (SetSelectionRange(start, end, direction))
    ScheduleSelectEvent();
}

static Position PositionForIndex(HTMLElement* inner_editor, unsigned index) {
  if (index == 0) {
    Node* node = NodeTraversal::Next(*inner_editor, inner_editor);
    if (node && node->IsTextNode())
      return Position(node, 0);
    return Position(inner_editor, 0);
  }
  unsigned remaining_characters_to_move_forward = index;
  Node* last_br_or_text = inner_editor;
  for (Node& node : NodeTraversal::DescendantsOf(*inner_editor)) {
    if (node.HasTagName(html_names::kBrTag)) {
      if (remaining_characters_to_move_forward == 0)
        return Position::BeforeNode(node);
      --remaining_characters_to_move_forward;
      last_br_or_text = &node;
      continue;
    }

    if (auto* text = DynamicTo<Text>(node)) {
      if (remaining_characters_to_move_forward < text->length())
        return Position(text, remaining_characters_to_move_forward);
      remaining_characters_to_move_forward -= text->length();
      last_br_or_text = &node;
      continue;
    }

    NOTREACHED_IN_MIGRATION();
  }
  DCHECK(last_br_or_text);
  return LastPositionInOrAfterNode(*last_br_or_text);
}

unsigned TextControlElement::IndexForPosition(HTMLElement* inner_editor,
                                              const Position& passed_position) {
  if (!inner_editor || !inner_editor->contains(passed_position.AnchorNode()) ||
      passed_position.IsNull())
    return 0;

  if (Position::BeforeNode(*inner_editor) == passed_position)
    return 0;

  unsigned index = 0;
  Node* start_node = passed_position.ComputeNodeBeforePosition();
  if (!start_node)
    start_node = passed_position.ComputeContainerNode();
  if (start_node == inner_editor && passed_position.IsAfterAnchor())
    start_node = inner_editor->lastChild();
  DCHECK(start_node);
  DCHECK(inner_editor->contains(start_node));

  for (Node* node = start_node; node;
       node = NodeTraversal::Previous(*node, inner_editor)) {
    if (auto* text_node = DynamicTo<Text>(node)) {
      int length = text_node->length();
      if (node == passed_position.ComputeContainerNode())
        index += std::min(length, passed_position.OffsetInContainerNode());
      else
        index += length;
      // Disregard the last auto added placeholder BrTag.
    } else if (node->HasTagName(html_names::kBrTag) &&
               node != inner_editor->lastChild()) {
      ++index;
    }
  }

  return index;
}

bool TextControlElement::ShouldApplySelectionCache() const {
  const auto& doc = GetDocument();
  return doc.FocusedElement() != this || doc.ShouldUpdateSelectionAfterLayout();
}

bool TextControlElement::SetSelectionRange(
    unsigned start,
    unsigned end,
    TextFieldSelectionDirection direction) {
  if (OpenShadowRoot() || !IsTextControl())
    return false;
  HTMLElement* inner_editor = EnsureInnerEditorElement();
  const unsigned editor_value_length = InnerEditorValue().length();
  end = std::min(end, editor_value_length);
  start = std::min(start, end);
  LocalFrame* frame = GetDocument().GetFrame();
  if (direction == kSelectionHasNoDirection && frame &&
      frame->GetEditor().Behavior().ShouldConsiderSelectionAsDirectional())
    direction = kSelectionHasForwardDirection;
  bool did_change = CacheSelection(start, end, direction);

  // TODO(crbug.com/927646): The focused element should always be connected, but
  // we fail to ensure so in some cases. Fix it.
  if (ShouldApplySelectionCache() || !isConnected()) {
    if (did_change) {
      ScheduleSelectionchangeEventOnThisOrDocument();
    }
    return did_change;
  }

  if (!frame || !inner_editor) {
    if (did_change) {
      ScheduleSelectionchangeEventOnThisOrDocument();
    }
    return did_change;
  }

  Position start_position = PositionForIndex(inner_editor, start);
  Position end_position =
      start == end ? start_position : PositionForIndex(inner_editor, end);

  DCHECK_EQ(start, IndexForPosition(inner_editor, start_position));
  DCHECK_EQ(end, IndexForPosition(inner_editor, end_position));

#if DCHECK_IS_ON()
  // startPosition and endPosition can be null position for example when
  // "-webkit-user-select: none" style attribute is specified.
  if (start_position.IsNotNull() && end_position.IsNotNull()) {
    DCHECK_EQ(start_position.AnchorNode()->OwnerShadowHost(), this);
    DCHECK_EQ(end_position.AnchorNode()->OwnerShadowHost(), this);
  }
#endif  // DCHECK_IS_ON()
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(direction == kSelectionHasBackwardDirection
                        ? end_position
                        : start_position)
          .Extend(direction == kSelectionHasBackwardDirection ? start_position
                                                              : end_position)
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetDoNotSetFocus(true)
          .SetIsDirectional(direction != kSelectionHasNoDirection)
          .Build());
  return did_change;
}

bool TextControlElement::CacheSelection(unsigned start,
                                        unsigned end,
                                        TextFieldSelectionDirection direction) {
  DCHECK_LE(start, end);
  bool did_change = cached_selection_start_ != start ||
                    cached_selection_end_ != end ||
                    cached_selection_direction_ != direction;
  cached_selection_start_ = start;
  cached_selection_end_ = end;
  cached_selection_direction_ = direction;
  return did_change;
}

VisiblePosition TextControlElement::VisiblePositionForIndex(int index) const {
  if (index <= 0)
    return VisiblePosition::FirstPositionInNode(*InnerEditorElement());
  Position start, end;
  bool selected = Range::selectNodeContents(InnerEditorElement(), start, end);
  if (!selected)
    return VisiblePosition();
  CharacterIterator it(start, end);
  it.Advance(index - 1);
  return CreateVisiblePosition(it.EndPosition(), TextAffinity::kUpstream);
}

unsigned TextControlElement::selectionStart() const {
  if (!IsTextControl())
    return 0;
  if (ShouldApplySelectionCache())
    return cached_selection_start_;

  ComputedSelection computed_selection;
  ComputeSelection(kStart, computed_selection);
  return computed_selection.start;
}

void TextControlElement::ComputeSelection(
    uint32_t flags,
    ComputedSelection& computed_selection) const {
  DCHECK(IsTextControl());
#if DCHECK_IS_ON()
  // This code does not set all values of `computed_selection`. Ensure they
  // are set to the default.
  DCHECK_EQ(0u, computed_selection.start);
  DCHECK_EQ(0u, computed_selection.end);
  DCHECK_EQ(kSelectionHasNoDirection, computed_selection.direction);
#endif
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return;

  // To avoid regression on speedometer benchmark[1] test, we should not
  // update layout tree in this code block.
  // [1] http://browserbench.org/Speedometer/
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());
  const SelectionInDOMTree& selection =
      frame->Selection().GetSelectionInDOMTree();
  if (flags & kStart) {
    computed_selection.start = IndexForPosition(
        InnerEditorElement(), selection.ComputeStartPosition());
  }
  if (flags & kEnd) {
    if (flags & kStart && !selection.IsRange()) {
      computed_selection.end = computed_selection.start;
    } else {
      computed_selection.end = IndexForPosition(InnerEditorElement(),
                                                selection.ComputeEndPosition());
    }
  }
  if (flags & kDirection && frame->Selection().IsDirectional()) {
    computed_selection.direction = (selection.IsAnchorFirst())
                                       ? kSelectionHasForwardDirection
                                       : kSelectionHasBackwardDirection;
  }
}

unsigned TextControlElement::selectionEnd() const {
  if (!IsTextControl())
    return 0;
  if (ShouldApplySelectionCache())
    return cached_selection_end_;
  ComputedSelection computed_selection;
  ComputeSelection(kEnd, computed_selection);
  return computed_selection.end;
}

static const AtomicString& DirectionString(
    TextFieldSelectionDirection direction) {
  DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, forward, ("forward"));
  DEFINE_STATIC_LOCAL(const AtomicString, backward, ("backward"));

  switch (direction) {
    case kSelectionHasNoDirection:
      return none;
    case kSelectionHasForwardDirection:
      return forward;
    case kSelectionHasBackwardDirection:
      return backward;
  }

  NOTREACHED_IN_MIGRATION();
  return none;
}

const AtomicString& TextControlElement::selectionDirection() const {
  // Ensured by HTMLInputElement::selectionDirectionForBinding().
  DCHECK(IsTextControl());
  if (ShouldApplySelectionCache())
    return DirectionString(cached_selection_direction_);
  ComputedSelection computed_selection;
  ComputeSelection(kDirection, computed_selection);
  return DirectionString(computed_selection.direction);
}

static inline void SetContainerAndOffsetForRange(Node* node,
                                                 int offset,
                                                 Node*& container_node,
                                                 int& offset_in_container) {
  if (node->IsTextNode()) {
    container_node = node;
    offset_in_container = offset;
  } else {
    container_node = node->parentNode();
    offset_in_container = node->NodeIndex() + offset;
  }
}

SelectionInDOMTree TextControlElement::Selection() const {
  if (!GetLayoutObject() || !IsTextControl())
    return SelectionInDOMTree();

  int start = cached_selection_start_;
  int end = cached_selection_end_;

  DCHECK_LE(start, end);
  HTMLElement* inner_text = InnerEditorElement();
  if (!inner_text)
    return SelectionInDOMTree();

  if (!inner_text->HasChildren()) {
    return SelectionInDOMTree::Builder()
        .Collapse(Position(inner_text, 0))
        .Build();
  }

  int offset = 0;
  Node* start_node = nullptr;
  Node* end_node = nullptr;
  for (Node& node : NodeTraversal::DescendantsOf(*inner_text)) {
    DCHECK(!node.hasChildren());
    DCHECK(node.IsTextNode() || IsA<HTMLBRElement>(node));
    int length = node.IsTextNode() ? Position::LastOffsetInNode(node) : 1;

    if (offset <= start && start <= offset + length)
      SetContainerAndOffsetForRange(&node, start - offset, start_node, start);

    if (offset <= end && end <= offset + length) {
      SetContainerAndOffsetForRange(&node, end - offset, end_node, end);
      break;
    }

    offset += length;
  }

  if (!start_node || !end_node)
    return SelectionInDOMTree();

  TextAffinity affinity = TextAffinity::kDownstream;
  if (GetDocument().FocusedElement() == this && GetDocument().GetFrame()) {
    const SelectionInDOMTree& selection =
        GetDocument().GetFrame()->Selection().GetSelectionInDOMTree();
    affinity = selection.Affinity();
  }

  return SelectionInDOMTree::Builder()
      .SetBaseAndExtent(Position(start_node, start), Position(end_node, end))
      .SetAffinity(affinity)
      .Build();
}

int TextControlElement::maxLength() const {
  int value;
  if (!ParseHTMLInteger(FastGetAttribute(html_names::kMaxlengthAttr), value))
    return -1;
  return value >= 0 ? value : -1;
}

int TextControlElement::minLength() const {
  int value;
  if (!ParseHTMLInteger(FastGetAttribute(html_names::kMinlengthAttr), value))
    return -1;
  return value >= 0 ? value : -1;
}

void TextControlElement::setMaxLength(int new_value,
                                      ExceptionState& exception_state) {
  int min = minLength();
  if (new_value < 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kIndexSizeError,
                                      "The value provided (" +
                                          String::Number(new_value) +
                                          ") is not positive or 0.");
  } else if (min >= 0 && new_value < min) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMinimumBound("maxLength", new_value,
                                                    min));
  } else {
    SetIntegralAttribute(html_names::kMaxlengthAttr, new_value);
  }
}

void TextControlElement::setMinLength(int new_value,
                                      ExceptionState& exception_state) {
  int max = maxLength();
  if (new_value < 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kIndexSizeError,
                                      "The value provided (" +
                                          String::Number(new_value) +
                                          ") is not positive or 0.");
  } else if (max >= 0 && new_value > max) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("minLength", new_value,
                                                    max));
  } else {
    SetIntegralAttribute(html_names::kMinlengthAttr, new_value);
  }
}

void TextControlElement::RestoreCachedSelection() {
  if (SetSelectionRange(cached_selection_start_, cached_selection_end_,
                        cached_selection_direction_))
    ScheduleSelectEvent();
}

void TextControlElement::SelectionChanged(bool user_triggered) {
  if (!GetLayoutObject() || !IsTextControl())
    return;

  // selectionStart() or selectionEnd() will return cached selection when this
  // node doesn't have focus.
  ComputedSelection computed_selection;
  ComputeSelection(kStart | kEnd | kDirection, computed_selection);
  CacheSelection(computed_selection.start, computed_selection.end,
                 computed_selection.direction);

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || !user_triggered)
    return;
  const SelectionInDOMTree& selection =
      frame->Selection().GetSelectionInDOMTree();
  if (!selection.IsRange())
    return;
  DispatchEvent(*Event::CreateBubble(event_type_names::kSelect));
}

void TextControlElement::ScheduleSelectEvent() {
  Event* event = Event::CreateBubble(event_type_names::kSelect);
  event->SetTarget(this);
  GetDocument().EnqueueAnimationFrameEvent(event);
}

void TextControlElement::ScheduleSelectionchangeEventOnThisOrDocument() {
  if (RuntimeEnabledFeatures::DispatchSelectionchangeEventPerElementEnabled()) {
    if (!IsInShadowTree()) {
      ScheduleSelectionchangeEvent();
    } else {
      GetDocument().ScheduleSelectionchangeEvent();
    }
  }
}

void TextControlElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kPlaceholderAttr) {
    UpdatePlaceholderText();
    UpdatePlaceholderVisibility();
    UseCounter::Count(GetDocument(), WebFeature::kPlaceholderAttribute);
  } else if (params.name == html_names::kReadonlyAttr ||
             params.name == html_names::kDisabledAttr) {
    DisabledOrReadonlyAttributeChanged(params.name);
    HTMLFormControlElementWithState::ParseAttribute(params);
    if (params.new_value.IsNull())
      return;

    if (HTMLElement* inner_editor = InnerEditorElement()) {
      if (auto* frame = GetDocument().GetFrame())
        frame->GetSpellChecker().RemoveSpellingAndGrammarMarkers(*inner_editor);
    }
  } else if (params.name == html_names::kSpellcheckAttr) {
    if (HTMLElement* inner_editor = InnerEditorElement()) {
      if (auto* frame = GetDocument().GetFrame()) {
        frame->GetSpellChecker().RespondToChangedEnablement(
            *inner_editor, IsSpellCheckingEnabled());
      }
    }
  } else {
    HTMLFormControlElementWithState::ParseAttribute(params);
  }
}

void TextControlElement::DisabledOrReadonlyAttributeChanged(
    const QualifiedName& attr) {
  if (Element* inner_editor = InnerEditorElement()) {
    inner_editor->SetNeedsStyleRecalc(
        kLocalStyleChange, StyleChangeReasonForTracing::FromAttribute(attr));
  }
}

bool TextControlElement::LastChangeWasUserEdit() const {
  if (!IsTextControl())
    return false;
  return last_change_was_user_edit_;
}

Node* TextControlElement::CreatePlaceholderBreakElement() const {
  return MakeGarbageCollected<HTMLBRElement>(GetDocument());
}

void TextControlElement::AddPlaceholderBreakElementIfNecessary() {
  HTMLElement* inner_editor = InnerEditorElement();
  if (inner_editor->GetLayoutObject() &&
      inner_editor->GetLayoutObject()->Style()->ShouldCollapseBreaks()) {
    return;
  }
  auto* last_child_text_node = DynamicTo<Text>(inner_editor->lastChild());
  if (!last_child_text_node)
    return;
  if (last_child_text_node->data().EndsWith('\n') ||
      last_child_text_node->data().EndsWith('\r'))
    inner_editor->AppendChild(CreatePlaceholderBreakElement());
}

void TextControlElement::SetInnerEditorValue(const String& value) {
  DCHECK(!OpenShadowRoot());
  if (!IsTextControl() || OpenShadowRoot())
    return;

  bool text_is_changed = value != InnerEditorValue();
  HTMLElement* inner_editor = EnsureInnerEditorElement();
  if (!text_is_changed && inner_editor->HasChildren())
    return;

  // If the last child is a trailing <br> that's appended below, remove it
  // first so as to enable setInnerText() fast path of updating a text node.
  if (IsA<HTMLBRElement>(inner_editor->lastChild()))
    inner_editor->RemoveChild(inner_editor->lastChild(), ASSERT_NO_EXCEPTION);

  // We don't use setTextContent.  It triggers unnecessary paint.
  if (value.empty())
    inner_editor->RemoveChildren();
  else
    ReplaceChildrenWithText(inner_editor, value, ASSERT_NO_EXCEPTION);

  // Add <br> so that we can put the caret at the next line of the last
  // newline.
  AddPlaceholderBreakElementIfNecessary();

  if (text_is_changed && GetLayoutObject()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
      cache->HandleTextFormControlChanged(this);
  }
}

String TextControlElement::InnerEditorValue() const {
  DCHECK(!OpenShadowRoot());
  HTMLElement* inner_editor = InnerEditorElement();
  if (!inner_editor || !IsTextControl())
    return g_empty_string;

  // Typically, innerEditor has 0 or one Text node followed by 0 or one <br>.
  if (!inner_editor->HasChildren())
    return g_empty_string;
  Node& first_child = *inner_editor->firstChild();
  if (auto* first_child_text_node = DynamicTo<Text>(first_child)) {
    Node* second_child = first_child.nextSibling();
    if (!second_child ||
        (!second_child->nextSibling() && IsA<HTMLBRElement>(*second_child)))
      return first_child_text_node->data();
  } else if (!first_child.nextSibling() && IsA<HTMLBRElement>(first_child)) {
    return g_empty_string;
  }

  StringBuilder result;
  for (Node& node : NodeTraversal::InclusiveDescendantsOf(*inner_editor)) {
    if (IsA<HTMLBRElement>(node)) {
      DCHECK_EQ(&node, inner_editor->lastChild());
      if (&node != inner_editor->lastChild())
        result.Append(kNewlineCharacter);
    } else if (auto* text_node = DynamicTo<Text>(node)) {
      result.Append(text_node->data());
    }
  }
  return result.ToString();
}

String TextControlElement::ValueWithHardLineBreaks() const {
  // FIXME: It's not acceptable to ignore the HardWrap setting when there is no
  // layoutObject.  While we have no evidence this has ever been a practical
  // problem, it would be best to fix it some day.
  HTMLElement* inner_text = InnerEditorElement();
  if (!inner_text || !IsTextControl())
    return Value();

  auto* layout_object = To<LayoutBlockFlow>(inner_text->GetLayoutObject());
  if (!layout_object)
    return Value();

  if (layout_object->IsLayoutNGObject()) {
    InlineCursor cursor(*layout_object);
    if (!cursor)
      return Value();
    const auto* mapping = InlineNode::GetOffsetMapping(layout_object);
    if (!mapping)
      return Value();
    Position break_position = GetNextSoftBreak(*mapping, cursor);
    StringBuilder result;
    for (Node& node : NodeTraversal::DescendantsOf(*inner_text)) {
      if (IsA<HTMLBRElement>(node)) {
        DCHECK_EQ(&node, inner_text->lastChild());
      } else if (auto* text_node = DynamicTo<Text>(node)) {
        String data = text_node->data();
        unsigned length = data.length();
        unsigned position = 0;
        while (break_position.AnchorNode() == node &&
               static_cast<unsigned>(break_position.OffsetInContainerNode()) <=
                   length) {
          unsigned break_offset = break_position.OffsetInContainerNode();
          if (break_offset > position) {
            result.Append(data, position, break_offset - position);
            position = break_offset;
            result.Append(kNewlineCharacter);
          }
          break_position = GetNextSoftBreak(*mapping, cursor);
        }
        result.Append(data, position, length - position);
      }
      while (break_position.AnchorNode() == node)
        break_position = GetNextSoftBreak(*mapping, cursor);
    }
    return result.ToString();
  }

  return Value();
}

TextControlElement* EnclosingTextControl(const Position& position) {
  DCHECK(position.IsNull() || position.IsOffsetInAnchor() ||
         position.ComputeContainerNode() ||
         !position.AnchorNode()->OwnerShadowHost() ||
         (position.AnchorNode()->parentNode() &&
          position.AnchorNode()->parentNode()->IsShadowRoot()));
  return EnclosingTextControl(position.ComputeContainerNode());
}

TextControlElement* EnclosingTextControl(const PositionInFlatTree& position) {
  Node* container = position.ComputeContainerNode();
  if (IsTextControl(container)) {
    // For example, #inner-editor@beforeAnchor reaches here.
    return ToTextControl(container);
  }
  return EnclosingTextControl(container);
}

TextControlElement* EnclosingTextControl(const Node* container) {
  if (!container)
    return nullptr;
  Element* ancestor = container->OwnerShadowHost();
  return ancestor && IsTextControl(*ancestor) &&
                 container->ContainingShadowRoot()->IsUserAgent()
             ? ToTextControl(ancestor)
             : nullptr;
}

String TextControlElement::DirectionForFormData() const {
  for (const HTMLElement* element = this; element;
       element = Traversal<HTMLElement>::FirstAncestor(*element)) {
    const AtomicString& dir_attribute_value =
        element->FastGetAttribute(html_names::kDirAttr);
    if (dir_attribute_value.IsNull()) {
      auto* input_element = DynamicTo<HTMLInputElement>(*this);
      if (input_element && input_element->IsTelephone()) {
        break;
      }
      continue;
    }

    if (EqualIgnoringASCIICase(dir_attribute_value, "rtl") ||
        EqualIgnoringASCIICase(dir_attribute_value, "ltr"))
      return dir_attribute_value;

    if (EqualIgnoringASCIICase(dir_attribute_value, "auto")) {
      return element->CachedDirectionality() == TextDirection::kRtl ? "rtl"
                                                                    : "ltr";
    }
  }

  return "ltr";
}

void TextControlElement::SetAutofillValue(const String& value,
                                          WebAutofillState autofill_state) {
  // Set the value trimmed to the max length of the field and dispatch the input
  // and change events.
  SetValue(value.Substring(0, maxLength()),
           TextFieldEventBehavior::kDispatchInputAndChangeEvent,
           TextControlSetValueSelection::kSetSelectionToEnd,
           value.empty() ? WebAutofillState::kNotFilled : autofill_state);
}

void TextControlElement::SetSuggestedValue(const String& value) {
  // Avoid calling maxLength() if possible as it's non-trivial.
  const String new_suggested_value =
      value.empty() ? value : value.Substring(0, maxLength());
  if (new_suggested_value == suggested_value_) {
    return;
  }
  suggested_value_ = new_suggested_value;

  // A null value indicates that the inner editor value should be shown, and a
  // non-null one indicates it should be hidden so that the suggested value can
  // be shown.
  if (auto* editor = InnerEditorElement()) {
    if (!value.IsNull() && !InnerEditorValue().empty()) {
      editor->SetVisibility(false);
    } else if (value.IsNull()) {
      editor->SetVisibility(true);
    }
  }

  HTMLElement* placeholder = UpdatePlaceholderText();
  if (!placeholder)
    return;

  UpdatePlaceholderVisibility();
  UpdatePlaceholderShadowPseudoId(*placeholder);
}

HTMLElement* TextControlElement::CreateInnerEditorElement() {
  DCHECK(!inner_editor_);
  inner_editor_ =
      MakeGarbageCollected<TextControlInnerEditorElement>(GetDocument());
  return inner_editor_.Get();
}

const String& TextControlElement::SuggestedValue() const {
  return suggested_value_;
}

void TextControlElement::ScheduleSelectionchangeEvent() {
  if (RuntimeEnabledFeatures::CoalesceSelectionchangeEventEnabled()) {
    if (has_scheduled_selectionchange_event_)
      return;
    has_scheduled_selectionchange_event_ = true;
    EnqueueEvent(*Event::CreateBubble(event_type_names::kSelectionchange),
                 TaskType::kMiscPlatformAPI);
  } else {
    EnqueueEvent(*Event::CreateBubble(event_type_names::kSelectionchange),
                 TaskType::kMiscPlatformAPI);
  }
}

void TextControlElement::Trace(Visitor* visitor) const {
  visitor->Trace(inner_editor_);
  HTMLFormControlElementWithState::Trace(visitor);
}

void TextControlElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    NodeCloningData& data) {
  const TextControlElement& source_element =
      static_cast<const TextControlElement&>(source);
  last_change_was_user_edit_ = source_element.last_change_was_user_edit_;
  interacted_state_ = source_element.interacted_state_;
  HTMLFormControlElement::CloneNonAttributePropertiesFrom(source, data);
}

ETextOverflow TextControlElement::ValueForTextOverflow() const {
  if (GetDocument().FocusedElement() == this)
    return ETextOverflow::kClip;
  return ComputedStyleRef().TextOverflow();
}

}  // namespace blink
