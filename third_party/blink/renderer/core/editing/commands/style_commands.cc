/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/style_commands.h"

#include "mojo/public/mojom/base/text_direction.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/editing/commands/apply_style_command.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_style_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_tri_state.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_font_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

void StyleCommands::ApplyStyle(LocalFrame& frame,
                               CSSPropertyValueSet* style,
                               InputEvent::InputType input_type) {
  const VisibleSelection& selection =
      frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  if (selection.IsNone())
    return;
  if (selection.IsCaret()) {
    frame.GetEditor().ComputeAndSetTypingStyle(style, input_type);
    return;
  }
  DCHECK(selection.IsRange()) << selection;
  if (!style)
    return;
  DCHECK(frame.GetDocument());
  MakeGarbageCollected<ApplyStyleCommand>(
      *frame.GetDocument(), MakeGarbageCollected<EditingStyle>(style),
      input_type)
      ->Apply();
}

void StyleCommands::ApplyStyleToSelection(LocalFrame& frame,
                                          CSSPropertyValueSet* style,
                                          InputEvent::InputType input_type) {
  if (!style || style->IsEmpty() || !frame.GetEditor().CanEditRichly())
    return;

  ApplyStyle(frame, style, input_type);
}

bool StyleCommands::ApplyCommandToFrame(LocalFrame& frame,
                                        EditorCommandSource source,
                                        InputEvent::InputType input_type,
                                        CSSPropertyValueSet* style) {
  // TODO(editing-dev): We don't call shouldApplyStyle when the source is DOM;
  // is there a good reason for that?
  switch (source) {
    case EditorCommandSource::kMenuOrKeyBinding:
      ApplyStyleToSelection(frame, style, input_type);
      return true;
    case EditorCommandSource::kDOM:
      ApplyStyle(frame, style, input_type);
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StyleCommands::ExecuteApplyStyle(LocalFrame& frame,
                                      EditorCommandSource source,
                                      InputEvent::InputType input_type,
                                      CSSPropertyID property_id,
                                      const String& property_value) {
  DCHECK(frame.GetDocument());
  auto* const style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  style->ParseAndSetProperty(property_id, property_value, /* important */ false,
                             frame.DomWindow()->GetSecureContextMode());
  return ApplyCommandToFrame(frame, source, input_type, style);
}

bool StyleCommands::ExecuteApplyStyle(LocalFrame& frame,
                                      EditorCommandSource source,
                                      InputEvent::InputType input_type,
                                      CSSPropertyID property_id,
                                      CSSValueID property_value) {
  auto* const style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  style->SetLonghandProperty(property_id, property_value);
  return ApplyCommandToFrame(frame, source, input_type, style);
}

bool StyleCommands::ExecuteBackColor(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source,
                                     const String& value) {
  return ExecuteApplyStyle(frame, source, InputEvent::InputType::kNone,
                           CSSPropertyID::kBackgroundColor, value);
}

bool StyleCommands::ExecuteForeColor(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source,
                                     const String& value) {
  return ExecuteApplyStyle(frame, source, InputEvent::InputType::kNone,
                           CSSPropertyID::kColor, value);
}

bool StyleCommands::ExecuteFontName(LocalFrame& frame,
                                    Event*,
                                    EditorCommandSource source,
                                    const String& value) {
  return ExecuteApplyStyle(frame, source, InputEvent::InputType::kNone,
                           CSSPropertyID::kFontFamily, value);
}

bool StyleCommands::ExecuteFontSize(LocalFrame& frame,
                                    Event*,
                                    EditorCommandSource source,
                                    const String& value) {
  CSSValueID size;
  if (!HTMLFontElement::CssValueFromFontSizeNumber(value, size))
    return false;
  return ExecuteApplyStyle(frame, source, InputEvent::InputType::kNone,
                           CSSPropertyID::kFontSize, size);
}

bool StyleCommands::ExecuteFontSizeDelta(LocalFrame& frame,
                                         Event*,
                                         EditorCommandSource source,
                                         const String& value) {
  // TODO(hjkim3323@gmail.com): Directly set EditingStyle::font_size_delta_
  // instead of setting it via CSS property
  return ExecuteApplyStyle(frame, source, InputEvent::InputType::kNone,
                           CSSPropertyID::kInternalFontSizeDelta, value);
}

bool StyleCommands::ExecuteMakeTextWritingDirectionLeftToRight(
    LocalFrame& frame,
    Event*,
    EditorCommandSource,
    const String&) {
  auto* const style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  style->SetLonghandProperty(CSSPropertyID::kUnicodeBidi, CSSValueID::kIsolate);
  style->SetLonghandProperty(CSSPropertyID::kDirection, CSSValueID::kLtr);
  ApplyStyle(frame, style, InputEvent::InputType::kFormatSetBlockTextDirection);
  return true;
}

bool StyleCommands::ExecuteMakeTextWritingDirectionNatural(LocalFrame& frame,
                                                           Event*,
                                                           EditorCommandSource,
                                                           const String&) {
  auto* const style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  style->SetLonghandProperty(CSSPropertyID::kUnicodeBidi, CSSValueID::kNormal);
  ApplyStyle(frame, style, InputEvent::InputType::kFormatSetBlockTextDirection);
  return true;
}

bool StyleCommands::ExecuteMakeTextWritingDirectionRightToLeft(
    LocalFrame& frame,
    Event*,
    EditorCommandSource,
    const String&) {
  auto* const style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  style->SetLonghandProperty(CSSPropertyID::kUnicodeBidi, CSSValueID::kIsolate);
  style->SetLonghandProperty(CSSPropertyID::kDirection, CSSValueID::kRtl);
  ApplyStyle(frame, style, InputEvent::InputType::kFormatSetBlockTextDirection);
  return true;
}

bool StyleCommands::SelectionStartHasStyle(LocalFrame& frame,
                                           CSSPropertyID property_id,
                                           const String& value) {
  const SecureContextMode secure_context_mode =
      frame.DomWindow()->GetSecureContextMode();

  EditingStyle* const style_to_check = MakeGarbageCollected<EditingStyle>(
      property_id, value, secure_context_mode);
  EditingStyle* const style_at_start =
      EditingStyleUtilities::CreateStyleAtSelectionStart(
          frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated(),
          property_id == CSSPropertyID::kBackgroundColor,
          style_to_check->Style());
  return style_to_check->TriStateOfStyle(frame.DomWindow(), style_at_start,
                                         secure_context_mode) !=
         EditingTriState::kFalse;
}

bool StyleCommands::ExecuteToggleStyle(LocalFrame& frame,
                                       EditorCommandSource source,
                                       InputEvent::InputType input_type,
                                       CSSPropertyID property_id,
                                       const char* off_value,
                                       const char* on_value) {
  // Style is considered present when
  // Mac: present at the beginning of selection
  // other: present throughout the selection
  const bool style_is_present =
      frame.GetEditor().Behavior().ShouldToggleStyleBasedOnStartOfSelection()
          ? SelectionStartHasStyle(frame, property_id, on_value)
          : EditingStyle::SelectionHasStyle(frame, property_id, on_value) ==
                EditingTriState::kTrue;

  EditingStyle* const style = MakeGarbageCollected<EditingStyle>(
      property_id, style_is_present ? off_value : on_value,
      frame.DomWindow()->GetSecureContextMode());
  return ApplyCommandToFrame(frame, source, input_type, style->Style());
}

bool StyleCommands::ExecuteToggleBold(LocalFrame& frame,
                                      Event*,
                                      EditorCommandSource source,
                                      const String&) {
  return ExecuteToggleStyle(frame, source, InputEvent::InputType::kFormatBold,
                            CSSPropertyID::kFontWeight, "normal", "bold");
}

bool StyleCommands::ExecuteToggleItalic(LocalFrame& frame,
                                        Event*,
                                        EditorCommandSource source,
                                        const String&) {
  return ExecuteToggleStyle(frame, source, InputEvent::InputType::kFormatItalic,
                            CSSPropertyID::kFontStyle, "normal", "italic");
}

bool StyleCommands::ExecuteSubscript(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source,
                                     const String&) {
  return ExecuteToggleStyle(frame, source,
                            InputEvent::InputType::kFormatSubscript,
                            CSSPropertyID::kVerticalAlign, "baseline", "sub");
}

bool StyleCommands::ExecuteSuperscript(LocalFrame& frame,
                                       Event*,
                                       EditorCommandSource source,
                                       const String&) {
  return ExecuteToggleStyle(frame, source,
                            InputEvent::InputType::kFormatSuperscript,
                            CSSPropertyID::kVerticalAlign, "baseline", "super");
}

bool StyleCommands::ExecuteUnscript(LocalFrame& frame,
                                    Event*,
                                    EditorCommandSource source,
                                    const String&) {
  return ExecuteApplyStyle(frame, source, InputEvent::InputType::kNone,
                           CSSPropertyID::kVerticalAlign, "baseline");
}

String StyleCommands::ComputeToggleStyleInList(EditingStyle& selection_style,
                                               CSSPropertyID property_id,
                                               const CSSValue& value) {
  const CSSValue& selected_css_value =
      *selection_style.Style()->GetPropertyCSSValue(property_id);
  if (auto* selected_value_list_original =
          DynamicTo<CSSValueList>(selected_css_value)) {
    CSSValueList& selected_css_value_list =
        *selected_value_list_original->Copy();
    if (!selected_css_value_list.RemoveAll(value))
      selected_css_value_list.Append(value);
    if (selected_css_value_list.length())
      return selected_css_value_list.CssText();
  } else if (selected_css_value.CssText() == "none") {
    return value.CssText();
  }
  return "none";
}

bool StyleCommands::ExecuteToggleStyleInList(LocalFrame& frame,
                                             EditorCommandSource source,
                                             InputEvent::InputType input_type,
                                             CSSPropertyID property_id,
                                             const CSSValue& value) {
  EditingStyle* const selection_style =
      EditingStyleUtilities::CreateStyleAtSelectionStart(
          frame.Selection().ComputeVisibleSelectionInDOMTree());
  if (!selection_style || !selection_style->Style())
    return false;

  const String new_style =
      ComputeToggleStyleInList(*selection_style, property_id, value);

  // TODO(editing-dev): We shouldn't be having to convert new style into text.
  // We should have setPropertyCSSValue.
  auto* const new_mutable_style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  new_mutable_style->ParseAndSetProperty(
      property_id, new_style, /* important */ false,
      frame.DomWindow()->GetSecureContextMode());
  return ApplyCommandToFrame(frame, source, input_type, new_mutable_style);
}

bool StyleCommands::ExecuteStrikethrough(LocalFrame& frame,
                                         Event*,
                                         EditorCommandSource source,
                                         const String&) {
  const CSSIdentifierValue& line_through =
      *CSSIdentifierValue::Create(CSSValueID::kLineThrough);
  return ExecuteToggleStyleInList(
      frame, source, InputEvent::InputType::kFormatStrikeThrough,
      CSSPropertyID::kWebkitTextDecorationsInEffect, line_through);
}

bool StyleCommands::ExecuteUnderline(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource source,
                                     const String&) {
  const CSSIdentifierValue& underline =
      *CSSIdentifierValue::Create(CSSValueID::kUnderline);
  return ExecuteToggleStyleInList(
      frame, source, InputEvent::InputType::kFormatUnderline,
      CSSPropertyID::kWebkitTextDecorationsInEffect, underline);
}

bool StyleCommands::ExecuteStyleWithCSS(LocalFrame& frame,
                                        Event*,
                                        EditorCommandSource,
                                        const String& value) {
  frame.GetEditor().SetShouldStyleWithCSS(
      !EqualIgnoringASCIICase(value, "false"));
  return true;
}

bool StyleCommands::ExecuteUseCSS(LocalFrame& frame,
                                  Event*,
                                  EditorCommandSource,
                                  const String& value) {
  frame.GetEditor().SetShouldStyleWithCSS(
      EqualIgnoringASCIICase(value, "false"));
  return true;
}

// State functions
EditingTriState StyleCommands::StateStyle(LocalFrame& frame,
                                          CSSPropertyID property_id,
                                          const char* desired_value) {
  if (frame.GetInputMethodController().GetActiveEditContext()) {
    return EditingTriState::kFalse;
  }

  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  if (frame.GetEditor().Behavior().ShouldToggleStyleBasedOnStartOfSelection()) {
    return SelectionStartHasStyle(frame, property_id, desired_value)
               ? EditingTriState::kTrue
               : EditingTriState::kFalse;
  }
  return EditingStyle::SelectionHasStyle(frame, property_id, desired_value);
}

EditingTriState StyleCommands::StateBold(LocalFrame& frame, Event*) {
  return StateStyle(frame, CSSPropertyID::kFontWeight, "bold");
}

EditingTriState StyleCommands::StateItalic(LocalFrame& frame, Event*) {
  return StateStyle(frame, CSSPropertyID::kFontStyle, "italic");
}

EditingTriState StyleCommands::StateStrikethrough(LocalFrame& frame, Event*) {
  return StateStyle(frame, CSSPropertyID::kWebkitTextDecorationsInEffect,
                    "line-through");
}

EditingTriState StyleCommands::StateStyleWithCSS(LocalFrame& frame, Event*) {
  if (frame.GetInputMethodController().GetActiveEditContext()) {
    return EditingTriState::kFalse;
  }

  return frame.GetEditor().ShouldStyleWithCSS() ? EditingTriState::kTrue
                                                : EditingTriState::kFalse;
}

EditingTriState StyleCommands::StateSubscript(LocalFrame& frame, Event*) {
  return StateStyle(frame, CSSPropertyID::kVerticalAlign, "sub");
}

EditingTriState StyleCommands::StateSuperscript(LocalFrame& frame, Event*) {
  return StateStyle(frame, CSSPropertyID::kVerticalAlign, "super");
}

bool StyleCommands::IsUnicodeBidiNestedOrMultipleEmbeddings(
    CSSValueID value_id) {
  return value_id == CSSValueID::kEmbed ||
         value_id == CSSValueID::kBidiOverride ||
         value_id == CSSValueID::kWebkitIsolate ||
         value_id == CSSValueID::kWebkitIsolateOverride ||
         value_id == CSSValueID::kWebkitPlaintext ||
         value_id == CSSValueID::kIsolate ||
         value_id == CSSValueID::kIsolateOverride ||
         value_id == CSSValueID::kPlaintext;
}

mojo_base::mojom::blink::TextDirection StyleCommands::TextDirectionForSelection(
    const VisibleSelection& selection,
    EditingStyle* typing_style,
    bool& has_nested_or_multiple_embeddings) {
  has_nested_or_multiple_embeddings = true;

  if (selection.IsNone())
    return mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;

  const Position position = MostForwardCaretPosition(selection.Start());

  const Node* anchor_node = position.AnchorNode();
  if (!anchor_node)
    return mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;

  Position end;
  if (selection.IsRange()) {
    end = MostBackwardCaretPosition(selection.End());

    DCHECK(end.GetDocument());
    const EphemeralRange caret_range(position.ParentAnchoredEquivalent(),
                                     end.ParentAnchoredEquivalent());
    for (Node& node : caret_range.Nodes()) {
      if (!node.IsStyledElement())
        continue;

      Element& element = To<Element>(node);
      const CSSComputedStyleDeclaration& style =
          *MakeGarbageCollected<CSSComputedStyleDeclaration>(&element);
      const CSSValue* unicode_bidi =
          style.GetPropertyCSSValue(CSSPropertyID::kUnicodeBidi);
      auto* unicode_bidi_identifier_value =
          DynamicTo<CSSIdentifierValue>(unicode_bidi);
      if (!unicode_bidi_identifier_value)
        continue;

      const CSSValueID unicode_bidi_value =
          unicode_bidi_identifier_value->GetValueID();
      if (IsUnicodeBidiNestedOrMultipleEmbeddings(unicode_bidi_value))
        return mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;
    }
  }

  if (selection.IsCaret()) {
    mojo_base::mojom::blink::TextDirection direction;
    if (typing_style && typing_style->GetTextDirection(direction)) {
      has_nested_or_multiple_embeddings = false;
      return direction;
    }
    anchor_node = selection.VisibleStart().DeepEquivalent().AnchorNode();
  }
  DCHECK(anchor_node);

  // The selection is either a caret with no typing attributes or a range in
  // which no embedding is added, so just use the start position to decide.
  const Node* block = EnclosingBlock(anchor_node);
  mojo_base::mojom::blink::TextDirection found_direction =
      mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;

  for (Node& runner : NodeTraversal::InclusiveAncestorsOf(*anchor_node)) {
    if (runner == block)
      break;
    if (!runner.IsStyledElement())
      continue;

    auto* element = To<Element>(&runner);
    const CSSComputedStyleDeclaration& style =
        *MakeGarbageCollected<CSSComputedStyleDeclaration>(element);
    const CSSValue* unicode_bidi =
        style.GetPropertyCSSValue(CSSPropertyID::kUnicodeBidi);
    auto* unicode_bidi_identifier_value =
        DynamicTo<CSSIdentifierValue>(unicode_bidi);
    if (!unicode_bidi_identifier_value)
      continue;

    const CSSValueID unicode_bidi_value =
        unicode_bidi_identifier_value->GetValueID();
    if (unicode_bidi_value == CSSValueID::kNormal)
      continue;

    if (unicode_bidi_value == CSSValueID::kBidiOverride)
      return mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;

    DCHECK(EditingStyleUtilities::IsEmbedOrIsolate(unicode_bidi_value))
        << static_cast<int>(unicode_bidi_value);
    const CSSValue* direction =
        style.GetPropertyCSSValue(CSSPropertyID::kDirection);
    auto* direction_identifier_value = DynamicTo<CSSIdentifierValue>(direction);
    if (!direction_identifier_value)
      continue;

    const CSSValueID direction_value = direction_identifier_value->GetValueID();
    if (direction_value != CSSValueID::kLtr &&
        direction_value != CSSValueID::kRtl)
      continue;

    if (found_direction !=
        mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION)
      return mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;

    // In the range case, make sure that the embedding element persists until
    // the end of the range.
    if (selection.IsRange() && !end.AnchorNode()->IsDescendantOf(element))
      return mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION;

    found_direction =
        direction_value == CSSValueID::kLtr
            ? mojo_base::mojom::blink::TextDirection::LEFT_TO_RIGHT
            : mojo_base::mojom::blink::TextDirection::RIGHT_TO_LEFT;
  }
  has_nested_or_multiple_embeddings = false;
  return found_direction;
}

EditingTriState StyleCommands::StateTextWritingDirection(
    LocalFrame& frame,
    mojo_base::mojom::blink::TextDirection direction) {
  if (frame.GetInputMethodController().GetActiveEditContext()) {
    return EditingTriState::kFalse;
  }

  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  bool has_nested_or_multiple_embeddings;
  mojo_base::mojom::blink::TextDirection selection_direction =
      TextDirectionForSelection(
          frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated(),
          frame.GetEditor().TypingStyle(), has_nested_or_multiple_embeddings);
  // TODO(editing-dev): We should be returning MixedTriState when
  // selectionDirection == direction && hasNestedOrMultipleEmbeddings
  return (selection_direction == direction &&
          !has_nested_or_multiple_embeddings)
             ? EditingTriState::kTrue
             : EditingTriState::kFalse;
}

EditingTriState StyleCommands::StateTextWritingDirectionLeftToRight(
    LocalFrame& frame,
    Event*) {
  return StateTextWritingDirection(
      frame, mojo_base::mojom::blink::TextDirection::LEFT_TO_RIGHT);
}

EditingTriState StyleCommands::StateTextWritingDirectionNatural(
    LocalFrame& frame,
    Event*) {
  return StateTextWritingDirection(
      frame, mojo_base::mojom::blink::TextDirection::UNKNOWN_DIRECTION);
}

EditingTriState StyleCommands::StateTextWritingDirectionRightToLeft(
    LocalFrame& frame,
    Event*) {
  return StateTextWritingDirection(
      frame, mojo_base::mojom::blink::TextDirection::RIGHT_TO_LEFT);
}

EditingTriState StyleCommands::StateUnderline(LocalFrame& frame, Event*) {
  return StateStyle(frame, CSSPropertyID::kWebkitTextDecorationsInEffect,
                    "underline");
}

// Value functions
String StyleCommands::SelectionStartCSSPropertyValue(
    LocalFrame& frame,
    CSSPropertyID property_id) {
  EditingStyle* const selection_style =
      EditingStyleUtilities::CreateStyleAtSelectionStart(
          frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated(),
          property_id == CSSPropertyID::kBackgroundColor);
  if (!selection_style || !selection_style->Style())
    return String();

  if (property_id == CSSPropertyID::kFontSize)
    return String::Number(selection_style->LegacyFontSize(frame.GetDocument()));
  return selection_style->Style()->GetPropertyValue(property_id);
}

String StyleCommands::ValueStyle(LocalFrame& frame, CSSPropertyID property_id) {
  if (frame.GetInputMethodController().GetActiveEditContext()) {
    return g_empty_string;
  }

  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  // TODO(editing-dev): Rather than retrieving the style at the start of the
  // current selection, we should retrieve the style present throughout the
  // selection for non-Mac platforms.
  return SelectionStartCSSPropertyValue(frame, property_id);
}

String StyleCommands::ValueBackColor(const EditorInternalCommand&,
                                     LocalFrame& frame,
                                     Event*) {
  return ValueStyle(frame, CSSPropertyID::kBackgroundColor);
}

String StyleCommands::ValueForeColor(const EditorInternalCommand&,
                                     LocalFrame& frame,
                                     Event*) {
  return ValueStyle(frame, CSSPropertyID::kColor);
}

String StyleCommands::ValueFontName(const EditorInternalCommand&,
                                    LocalFrame& frame,
                                    Event*) {
  return ValueStyle(frame, CSSPropertyID::kFontFamily);
}

String StyleCommands::ValueFontSize(const EditorInternalCommand&,
                                    LocalFrame& frame,
                                    Event*) {
  return ValueStyle(frame, CSSPropertyID::kFontSize);
}

String StyleCommands::ValueFontSizeDelta(const EditorInternalCommand&,
                                         LocalFrame& frame,
                                         Event*) {
  return ValueStyle(frame, CSSPropertyID::kInternalFontSizeDelta);
}

}  // namespace blink
