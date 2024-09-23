/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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

#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/events/before_text_inserted_event.h"
#include "third_party/blink/renderer/core/events/drag_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/forms/layout_text_control_multi_line.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using mojom::blink::FormControlType;

static const unsigned kDefaultRows = 2;
static const unsigned kDefaultCols = 20;

static bool is_default_font_prewarmed_ = false;

static inline unsigned ComputeLengthForAPIValue(const String& text) {
  unsigned length = text.length();
  unsigned crlf_count = 0;
  for (unsigned i = 0; i < length; ++i) {
    if (text[i] == '\r' && i + 1 < length && text[i + 1] == '\n')
      crlf_count++;
  }
  return text.length() - crlf_count;
}

static inline void ReplaceCRWithNewLine(String& text) {
  text.Replace("\r\n", "\n");
  text.Replace('\r', '\n');
}

HTMLTextAreaElement::HTMLTextAreaElement(Document& document)
    : TextControlElement(html_names::kTextareaTag, document),
      rows_(kDefaultRows),
      cols_(kDefaultCols),
      wrap_(kSoftWrap),
      is_dirty_(false),
      is_placeholder_visible_(false) {
  EnsureUserAgentShadowRoot();

  if (!is_default_font_prewarmed_) {
    if (Settings* settings = document.GetSettings()) {
      // Prewarm 'monospace', the default font family for `<textarea>`. The
      // default language should be fine for this purpose because most users set
      // the same family for all languages.
      FontCache::PrewarmFamily(settings->GetGenericFontFamilySettings().Fixed(
          LayoutLocale::GetDefault().GetScript()));
      is_default_font_prewarmed_ = true;
    }
  }
}

void HTMLTextAreaElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  root.AppendChild(CreateInnerEditorElement());
}

FormControlType HTMLTextAreaElement::FormControlType() const {
  return FormControlType::kTextArea;
}

const AtomicString& HTMLTextAreaElement::FormControlTypeAsString() const {
  DEFINE_STATIC_LOCAL(const AtomicString, textarea, ("textarea"));
  return textarea;
}

FormControlState HTMLTextAreaElement::SaveFormControlState() const {
  return is_dirty_ ? FormControlState(Value()) : FormControlState();
}

void HTMLTextAreaElement::RestoreFormControlState(
    const FormControlState& state) {
  SetValue(state[0]);
}

int HTMLTextAreaElement::scrollWidth() {
  if (SuggestedValue().empty())
    return TextControlElement::scrollWidth();
  // If in preview state, fake the scroll width to prevent that any information
  // about the suggested content can be derived from the size.
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  auto* editor = InnerEditorElement();
  auto* editor_box = editor ? editor->GetLayoutBox() : nullptr;
  auto* box = GetLayoutBox();
  if (!box || !editor_box)
    return TextControlElement::scrollWidth();
  LayoutUnit width =
      editor_box->ClientWidth() + box->PaddingLeft() + box->PaddingRight();
  return AdjustForAbsoluteZoom::AdjustLayoutUnit(width, box->StyleRef())
      .Round();
}

int HTMLTextAreaElement::scrollHeight() {
  if (SuggestedValue().empty())
    return TextControlElement::scrollHeight();
  // If in preview state, fake the scroll height to prevent that any
  // information about the suggested content can be derived from the size.
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  auto* editor = InnerEditorElement();
  auto* editor_box = editor ? editor->GetLayoutBox() : nullptr;
  auto* box = GetLayoutBox();
  if (!box || !editor_box)
    return TextControlElement::scrollHeight();
  LayoutUnit height =
      editor_box->ClientHeight() + box->PaddingTop() + box->PaddingBottom();
  return AdjustForAbsoluteZoom::AdjustLayoutUnit(height, box->StyleRef())
      .Round();
}

void HTMLTextAreaElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (is_dirty_)
    SetInnerEditorValue(Value());
  else
    SetNonDirtyValue(defaultValue(), TextControlSetValueSelection::kClamp);
}

bool HTMLTextAreaElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kAlignAttr) {
    // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
    // See http://bugs.webkit.org/show_bug.cgi?id=7075
    return false;
  }

  if (name == html_names::kWrapAttr)
    return true;
  return TextControlElement::IsPresentationAttribute(name);
}

void HTMLTextAreaElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWrapAttr) {
    if (ShouldWrapText()) {
      // Longhands of `white-space: pre-wrap`.
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWhiteSpaceCollapse, CSSValueID::kPreserve);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kTextWrapMode, CSSValueID::kWrap);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kOverflowWrap, CSSValueID::kBreakWord);
    } else {
      // Longhands of `white-space: pre`.
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWhiteSpaceCollapse, CSSValueID::kPreserve);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kTextWrapMode, CSSValueID::kNowrap);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kOverflowWrap, CSSValueID::kNormal);
    }
  } else {
    TextControlElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void HTMLTextAreaElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == html_names::kRowsAttr) {
    unsigned rows = 0;
    if (value.empty() || !ParseHTMLNonNegativeInteger(value, rows) ||
        rows <= 0 || rows > 0x7fffffffu)
      rows = kDefaultRows;
    if (rows_ != rows) {
      rows_ = rows;
      if (GetLayoutObject()) {
        GetLayoutObject()
            ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
                layout_invalidation_reason::kAttributeChanged);
      }
    }
  } else if (name == html_names::kColsAttr) {
    unsigned cols = 0;
    if (value.empty() || !ParseHTMLNonNegativeInteger(value, cols) ||
        cols <= 0 || cols > 0x7fffffffu)
      cols = kDefaultCols;
    if (cols_ != cols) {
      cols_ = cols;
      if (LayoutObject* layout_object = GetLayoutObject()) {
        layout_object
            ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
                layout_invalidation_reason::kAttributeChanged);
      }
    }
  } else if (name == html_names::kWrapAttr) {
    // The virtual/physical values were a Netscape extension of HTML 3.0, now
    // deprecated.  The soft/hard /off values are a recommendation for HTML 4
    // extension by IE and NS 4.
    WrapMethod wrap;
    if (EqualIgnoringASCIICase(value, "physical") ||
        EqualIgnoringASCIICase(value, "hard") ||
        EqualIgnoringASCIICase(value, "on"))
      wrap = kHardWrap;
    else if (EqualIgnoringASCIICase(value, "off"))
      wrap = kNoWrap;
    else
      wrap = kSoftWrap;
    if (wrap != wrap_) {
      wrap_ = wrap;
      if (LayoutObject* layout_object = GetLayoutObject()) {
        layout_object
            ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
                layout_invalidation_reason::kAttributeChanged);
      }
    }
  } else if (name == html_names::kAccesskeyAttr) {
    // ignore for the moment
  } else if (name == html_names::kMaxlengthAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kTextAreaMaxLength);
    SetNeedsValidityCheck();
  } else if (name == html_names::kMinlengthAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kTextAreaMinLength);
    SetNeedsValidityCheck();
  } else {
    TextControlElement::ParseAttribute(params);
  }
}

LayoutObject* HTMLTextAreaElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutTextControlMultiLine>(this);
}

void HTMLTextAreaElement::AppendToFormData(FormData& form_data) {
  if (GetName().empty())
    return;

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kForm);

  const String& text =
      (wrap_ == kHardWrap) ? ValueWithHardLineBreaks() : Value();
  form_data.AppendFromElement(GetName(), text);

  const AtomicString& dirname_attr_value =
      FastGetAttribute(html_names::kDirnameAttr);
  if (!dirname_attr_value.IsNull())
    form_data.AppendFromElement(dirname_attr_value, DirectionForFormData());
}

void HTMLTextAreaElement::ResetImpl() {
  SetNonDirtyValue(defaultValue(),
                   TextControlSetValueSelection::kSetSelectionToEnd);
  HTMLFormControlElementWithState::ResetImpl();
}

bool HTMLTextAreaElement::HasCustomFocusLogic() const {
  return true;
}

bool HTMLTextAreaElement::IsKeyboardFocusable(
    UpdateBehavior update_behavior) const {
  // If a given text area can be focused at all, then it will always be keyboard
  // focusable, unless it has a negative tabindex set.
  return IsFocusable(update_behavior) && tabIndex() >= 0;
}

bool HTMLTextAreaElement::MayTriggerVirtualKeyboard() const {
  return true;
}

void HTMLTextAreaElement::UpdateSelectionOnFocus(
    SelectionBehaviorOnFocus selection_behavior,
    const FocusOptions* options) {
  switch (selection_behavior) {
    case SelectionBehaviorOnFocus::kReset:  // Fallthrough.
    case SelectionBehaviorOnFocus::kRestore:
      RestoreCachedSelection();
      break;
    case SelectionBehaviorOnFocus::kNone:
      return;
  }
  if (!options->preventScroll()) {
    if (GetDocument().GetFrame())
      GetDocument().GetFrame()->Selection().RevealSelection();
  }
}

void HTMLTextAreaElement::DefaultEventHandler(Event& event) {
  if (GetLayoutObject() &&
      (IsA<MouseEvent>(event) || IsA<DragEvent>(event) ||
       event.HasInterface(event_interface_names::kWheelEvent) ||
       event.type() == event_type_names::kBlur)) {
    ForwardEvent(event);
  } else if (GetLayoutObject() && event.IsBeforeTextInsertedEvent()) {
    HandleBeforeTextInsertedEvent(
        static_cast<BeforeTextInsertedEvent*>(&event));
  }

  TextControlElement::DefaultEventHandler(event);
}

void HTMLTextAreaElement::SubtreeHasChanged() {
#if DCHECK_IS_ON()
  // The innerEditor should have either Text nodes or a placeholder break
  // element. If we see other nodes, it's a bug in editing code and we should
  // fix it.
  Element* inner_editor = InnerEditorElement();
  for (Node& node : NodeTraversal::DescendantsOf(*inner_editor)) {
    if (node.IsTextNode())
      continue;
    DCHECK(IsA<HTMLBRElement>(node));
    DCHECK_EQ(&node, inner_editor->lastChild());
  }
#endif
  AddPlaceholderBreakElementIfNecessary();
  SetValueBeforeFirstUserEditIfNotSet();
  UpdateValue();
  CheckIfValueWasReverted(Value());
  SetNeedsValidityCheck();
  SetAutofillState(WebAutofillState::kNotFilled);
  UpdatePlaceholderVisibility();

  if (HasDirectionAuto() ||
      !RuntimeEnabledFeatures::TextInputNotAlwaysDirAutoEnabled()) {
    // When typing in a textarea, childrenChanged is not called, so we need to
    // force the directionality check.
    CalculateAndAdjustAutoDirectionality();
  }

  if (!IsFocused())
    return;

  DCHECK(GetDocument().IsActive());
  if (InnerEditorValue().empty()) {
    GetDocument().GetPage()->GetChromeClient().DidClearValueInTextField(*this);
  }
  GetDocument().GetPage()->GetChromeClient().DidChangeValueInTextField(*this);
}

void HTMLTextAreaElement::HandleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent* event) {
  DCHECK(event);
  DCHECK(GetLayoutObject());
  int signed_max_length = maxLength();
  if (signed_max_length < 0)
    return;
  unsigned unsigned_max_length = static_cast<unsigned>(signed_max_length);

  const String& current_value = InnerEditorValue();
  unsigned current_length = ComputeLengthForAPIValue(current_value);
  if (current_length + ComputeLengthForAPIValue(event->GetText()) <
      unsigned_max_length)
    return;

  // selectionLength represents the selection length of this text field to be
  // removed by this insertion.
  // If the text field has no focus, we don't need to take account of the
  // selection length. The selection is the source of text drag-and-drop in
  // that case, and nothing in the text field will be removed.
  unsigned selection_length = 0;
  if (IsFocused()) {
    // TODO(editing-dev): Use of UpdateStyleAndLayout
    // needs to be audited.  See http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kForm);

    selection_length = ComputeLengthForAPIValue(
        GetDocument().GetFrame()->Selection().SelectedText());
  }
  DCHECK_GE(current_length, selection_length);
  unsigned base_length = current_length - selection_length;
  unsigned appendable_length =
      unsigned_max_length > base_length ? unsigned_max_length - base_length : 0;
  event->SetText(SanitizeUserInputValue(event->GetText(), appendable_length));

  if (selection_length == current_length && selection_length != 0 &&
      !event->GetText().empty()) {
    GetDocument().GetPage()->GetChromeClient().DidClearValueInTextField(*this);
  }
}

String HTMLTextAreaElement::SanitizeUserInputValue(const String& proposed_value,
                                                   unsigned max_length) {
  unsigned submission_length = 0;
  unsigned i = 0;
  for (; i < proposed_value.length(); ++i) {
    if (proposed_value[i] == '\r' && i + 1 < proposed_value.length() &&
        proposed_value[i + 1] == '\n')
      continue;
    ++submission_length;
    if (submission_length == max_length) {
      ++i;
      break;
    }
    if (submission_length > max_length)
      break;
  }
  if (i > 0 && U16_IS_LEAD(proposed_value[i - 1]))
    --i;
  return proposed_value.Left(i);
}

void HTMLTextAreaElement::UpdateValue() {
  value_ = InnerEditorValue();
  NotifyFormStateChanged();
  is_dirty_ = true;
  UpdatePlaceholderVisibility();
}

String HTMLTextAreaElement::Value() const {
  return value_;
}

void HTMLTextAreaElement::setValueForBinding(const String& value) {
  String old_value = this->Value();
  bool was_autofilled = IsAutofilled();
  bool value_changed = old_value != value;
  SetValue(value, TextFieldEventBehavior::kDispatchNoEvent,
           TextControlSetValueSelection::kSetSelectionToEnd,
           was_autofilled && !value_changed ? WebAutofillState::kAutofilled
                                            : WebAutofillState::kNotFilled);
  if (Page* page = GetDocument().GetPage(); page && value_changed) {
    page->GetChromeClient().JavaScriptChangedValue(*this, old_value,
                                                   was_autofilled);
  }
}

void HTMLTextAreaElement::SetValue(const String& value,
                                   TextFieldEventBehavior event_behavior,
                                   TextControlSetValueSelection selection,
                                   WebAutofillState autofill_state) {
  SetValueCommon(value, event_behavior, selection, autofill_state);
  is_dirty_ = true;
}

void HTMLTextAreaElement::SetNonDirtyValue(
    const String& value,
    TextControlSetValueSelection selection) {
  SetValueCommon(value, TextFieldEventBehavior::kDispatchNoEvent, selection,
                 WebAutofillState::kNotFilled);
  is_dirty_ = false;
}

void HTMLTextAreaElement::SetValueCommon(const String& new_value,
                                         TextFieldEventBehavior event_behavior,
                                         TextControlSetValueSelection selection,
                                         WebAutofillState autofill_state) {
  // Code elsewhere normalizes line endings added by the user via the keyboard
  // or pasting.  We normalize line endings coming from JavaScript here.
  String normalized_value = new_value;
  ReplaceCRWithNewLine(normalized_value);

  // Clear the suggested value. Use the base class version to not trigger a view
  // update.
  TextControlElement::SetSuggestedValue(String());

  // Return early because we don't want to trigger other side effects when the
  // value isn't changing. This is interoperable.
  if (normalized_value == Value())
    return;

  // selectionStart and selectionEnd values can be changed by
  // SetInnerEditorValue(). We need to get them before SetInnerEditorValue() to
  // clamp them later in a case of kClamp.
  const bool is_clamp = selection == TextControlSetValueSelection::kClamp;
  const unsigned selection_start = is_clamp ? selectionStart() : 0;
  const unsigned selection_end = is_clamp ? selectionEnd() : 0;

  if (event_behavior != TextFieldEventBehavior::kDispatchNoEvent)
    SetValueBeforeFirstUserEditIfNotSet();
  value_ = normalized_value;
  SetInnerEditorValue(value_);
  if (event_behavior == TextFieldEventBehavior::kDispatchNoEvent)
    SetLastChangeWasNotUserEdit();
  else
    CheckIfValueWasReverted(value_);
  UpdatePlaceholderVisibility();
  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kControlValue));
  SetNeedsValidityCheck();
  if (selection == TextControlSetValueSelection::kSetSelectionToEnd) {
    // Set the caret to the end of the text value except for initialize.
    unsigned end_of_string = value_.length();
    SetSelectionRange(end_of_string, end_of_string);
  } else if (selection == TextControlSetValueSelection::kSetSelectionToStart) {
    // Set the caret to the start of the text value.
    SetSelectionRange(0, 0);
  } else if (is_clamp) {
    const unsigned end_of_string = value_.length();
    SetSelectionRange(std::min(end_of_string, selection_start),
                      std::min(end_of_string, selection_end));
  }

  SetAutofillState(autofill_state);
  NotifyFormStateChanged();
  switch (event_behavior) {
    case TextFieldEventBehavior::kDispatchChangeEvent:
      DispatchFormControlChangeEvent();
      break;

    case TextFieldEventBehavior::kDispatchInputEvent:
      DispatchInputEvent();
      break;

    case TextFieldEventBehavior::kDispatchInputAndChangeEvent:
      DispatchInputEvent();
      DispatchFormControlChangeEvent();
      break;

    case TextFieldEventBehavior::kDispatchNoEvent:
      break;
  }

  if (!RuntimeEnabledFeatures::AllowJavaScriptToResetAutofillStateEnabled()) {
    // We set the Autofilled state again because setting the autofill value
    // triggers JavaScript events and the site may override the autofilled
    // value, which resets the autofill state. Even if the website modifies the
    // form control element's content during the autofill operation, we want the
    // state to show as autofilled.
    // If AllowJavaScriptToResetAutofillState is enabled, the WebAutofillClient
    // will monitor JavaScript induced changes and take care of resetting the
    // autofill state when appropriate.
    SetAutofillState(autofill_state);
  }
}

String HTMLTextAreaElement::defaultValue() const {
  StringBuilder value;

  // Since there may be comments, ignore nodes other than text nodes.
  for (Node* n = firstChild(); n; n = n->nextSibling()) {
    if (auto* text_node = DynamicTo<Text>(n))
      value.Append(text_node->data());
  }

  return value.ToString();
}

void HTMLTextAreaElement::setDefaultValue(const String& default_value) {
  setTextContent(default_value);
}

void HTMLTextAreaElement::SetSuggestedValue(const String& value) {
  SetAutofillState(!value.empty() ? WebAutofillState::kPreviewed
                                  : WebAutofillState::kNotFilled);
  TextControlElement::SetSuggestedValue(value);
  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kControlValue));
}

String HTMLTextAreaElement::validationMessage() const {
  if (!willValidate())
    return String();

  if (CustomError())
    return CustomValidationMessage();

  if (ValueMissing())
    return GetLocale().QueryString(IDS_FORM_VALIDATION_VALUE_MISSING);

  if (TooLong()) {
    return GetLocale().ValidationMessageTooLongText(Value().length(),
                                                    maxLength());
  }

  if (TooShort()) {
    return GetLocale().ValidationMessageTooShortText(Value().length(),
                                                     minLength());
  }

  return String();
}

bool HTMLTextAreaElement::ValueMissing() const {
  // We should not call value() for performance.
  return ValueMissing(nullptr);
}

bool HTMLTextAreaElement::ValueMissing(const String* value) const {
  // For textarea elements, the value is missing only if it is mutable.
  // https://html.spec.whatwg.org/multipage/form-elements.html#attr-textarea-required
  return IsRequiredFormControl() && !IsDisabledOrReadOnly() &&
         (value ? *value : this->Value()).empty();
}

bool HTMLTextAreaElement::TooLong() const {
  // We should not call value() for performance.
  return willValidate() && TooLong(nullptr, kCheckDirtyFlag);
}

bool HTMLTextAreaElement::TooShort() const {
  // We should not call value() for performance.
  return willValidate() && TooShort(nullptr, kCheckDirtyFlag);
}

bool HTMLTextAreaElement::TooLong(const String* value,
                                  NeedsToCheckDirtyFlag check) const {
  // Return false for the default value or value set by script even if it is
  // longer than maxLength.
  if (check == kCheckDirtyFlag && !LastChangeWasUserEdit())
    return false;

  int max = maxLength();
  if (max < 0)
    return false;
  unsigned len =
      value ? ComputeLengthForAPIValue(*value) : this->Value().length();
  return len > static_cast<unsigned>(max);
}

bool HTMLTextAreaElement::TooShort(const String* value,
                                   NeedsToCheckDirtyFlag check) const {
  // Return false for the default value or value set by script even if it is
  // shorter than minLength.
  if (check == kCheckDirtyFlag && !LastChangeWasUserEdit())
    return false;

  int min = minLength();
  if (min <= 0)
    return false;
  // An empty string is excluded from minlength check.
  unsigned len =
      value ? ComputeLengthForAPIValue(*value) : this->Value().length();
  return len > 0 && len < static_cast<unsigned>(min);
}

bool HTMLTextAreaElement::IsValidValue(const String& candidate) const {
  return !ValueMissing(&candidate) && !TooLong(&candidate, kIgnoreDirtyFlag) &&
         !TooShort(&candidate, kIgnoreDirtyFlag);
}

void HTMLTextAreaElement::AccessKeyAction(SimulatedClickCreationScope) {
  Focus(FocusParams(FocusTrigger::kUserGesture));
}

void HTMLTextAreaElement::setCols(unsigned cols) {
  SetUnsignedIntegralAttribute(html_names::kColsAttr,
                               cols ? cols : kDefaultCols, kDefaultCols);
}

void HTMLTextAreaElement::setRows(unsigned rows) {
  SetUnsignedIntegralAttribute(html_names::kRowsAttr,
                               rows ? rows : kDefaultRows, kDefaultRows);
}

bool HTMLTextAreaElement::MatchesReadOnlyPseudoClass() const {
  return IsDisabledOrReadOnly();
}

bool HTMLTextAreaElement::MatchesReadWritePseudoClass() const {
  return !IsDisabledOrReadOnly();
}

void HTMLTextAreaElement::SetPlaceholderVisibility(bool visible) {
  is_placeholder_visible_ = visible;
}

void HTMLTextAreaElement::CreateInnerEditorElementIfNecessary() const {
  // HTMLTextArea immediately creates the inner-editor, so this function should
  // never be called.
  NOTREACHED_IN_MIGRATION();
}

bool HTMLTextAreaElement::IsInnerEditorValueEmpty() const {
  return InnerEditorValue().empty();
}

HTMLElement* HTMLTextAreaElement::UpdatePlaceholderText() {
  HTMLElement* placeholder = PlaceholderElement();
  const String placeholder_text = GetPlaceholderValue();
  const bool is_suggested_value = !SuggestedValue().empty();
  if (!is_suggested_value && !FastHasAttribute(html_names::kPlaceholderAttr)) {
    if (placeholder)
      UserAgentShadowRoot()->RemoveChild(placeholder);
    return nullptr;
  }
  if (!placeholder) {
    auto* new_element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
    placeholder = new_element;
    placeholder->SetShadowPseudoId(
        shadow_element_names::kPseudoInputPlaceholder);
    placeholder->setAttribute(html_names::kIdAttr,
                              shadow_element_names::kIdPlaceholder);
    placeholder->SetInlineStyleProperty(
        CSSPropertyID::kDisplay,
        IsPlaceholderVisible() ? CSSValueID::kBlock : CSSValueID::kNone, true);
    UserAgentShadowRoot()->InsertBefore(placeholder, InnerEditorElement());
  }
  if (is_suggested_value) {
    placeholder->SetInlineStyleProperty(CSSPropertyID::kUserSelect,
                                        CSSValueID::kNone, true);
  } else {
    placeholder->RemoveInlineStyleProperty(CSSPropertyID::kUserSelect);
  }
  String normalized_value = placeholder_text;
  // https://html.spec.whatwg.org/multipage/form-elements.html#attr-textarea-placeholder
  ReplaceCRWithNewLine(normalized_value);
  placeholder->setTextContent(normalized_value);
  return placeholder;
}

String HTMLTextAreaElement::GetPlaceholderValue() const {
  return !SuggestedValue().empty()
             ? SuggestedValue()
             : FastGetAttribute(html_names::kPlaceholderAttr);
}

bool HTMLTextAreaElement::IsInteractiveContent() const {
  return true;
}

void HTMLTextAreaElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    NodeCloningData& data) {
  const auto& source_element = To<HTMLTextAreaElement>(source);
  SetValueCommon(source_element.Value(),
                 TextFieldEventBehavior::kDispatchNoEvent,
                 TextControlSetValueSelection::kSetSelectionToStart,
                 source_element.GetAutofillState());
  is_dirty_ = source_element.is_dirty_;
  TextControlElement::CloneNonAttributePropertiesFrom(source, data);
}

String HTMLTextAreaElement::DefaultToolTip() const {
  if (FastHasAttribute(html_names::kNovalidateAttr))
    return String();
  return validationMessage();
}

void HTMLTextAreaElement::SetFocused(bool is_focused,
                                     mojom::blink::FocusType focus_type) {
  // See comment in HTMLInputElement::SetFocused.
  if (UserHasEditedTheField()) {
    SetUserHasEditedTheFieldAndBlurred();
  }
  TextControlElement::SetFocused(is_focused, focus_type);
}

}  // namespace blink
