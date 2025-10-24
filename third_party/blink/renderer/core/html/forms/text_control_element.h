/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009, 2010, 2011 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_TEXT_CONTROL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_TEXT_CONTROL_ELEMENT_H_

#include "base/auto_reset.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element_with_state.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/style/text_overflow_data.h"

namespace blink {

class ExceptionState;
class V8SelectionMode;
class FormControlRange;

enum TextFieldSelectionDirection {
  kSelectionHasNoDirection,
  kSelectionHasForwardDirection,
  kSelectionHasBackwardDirection
};
enum class TextFieldEventBehavior {
  kDispatchNoEvent,
  kDispatchChangeEvent,
  kDispatchInputEvent,
  kDispatchInputAndChangeEvent
};

enum class TextControlSetValueSelection {
  kSetSelectionToStart,
  kSetSelectionToEnd,
  kClamp,
  kDoNotSet,
};

class CORE_EXPORT TextControlElement : public HTMLFormControlElementWithState {
 public:
  // Common flag for HTMLInputElement::tooLong(),
  // HTMLTextAreaElement::tooLong(),
  // HTMLInputElement::tooShort() and HTMLTextAreaElement::tooShort().
  enum NeedsToCheckDirtyFlag { kCheckDirtyFlag, kIgnoreDirtyFlag };

  ~TextControlElement() override;

  void ForwardEvent(Event&);

  void SetFocused(bool, mojom::blink::FocusType) override;

  bool IsRichlyEditableForAccessibility() const override { return false; }

  // The derived class should return true if placeholder processing is needed.
  virtual bool IsPlaceholderVisible() const = 0;
  virtual void SetPlaceholderVisibility(bool) = 0;
  virtual bool SupportsPlaceholder() const = 0;
  String StrippedPlaceholder() const;
  HTMLElement* PlaceholderElement() const;
  void UpdatePlaceholderVisibility();
  void UpdatePlaceholderShadowPseudoId(HTMLElement& placeholder);
  virtual String GetPlaceholderValue() const = 0;

  VisiblePosition VisiblePositionForIndex(int) const;
  unsigned selectionStart() const;
  unsigned selectionEnd() const;
  const AtomicString& selectionDirection() const;
  void setSelectionStart(unsigned);
  void setSelectionEnd(unsigned);
  void setSelectionDirection(const String&);
  void select();
  virtual void setRangeText(const String& replacement, ExceptionState&);
  virtual void setRangeText(const String& replacement,
                            unsigned start,
                            unsigned end,
                            const V8SelectionMode& selection_mode,
                            ExceptionState&);
  // Web-exposed setSelectionRange() function. This schedule to dispatch
  // 'select' event.
  void setSelectionRangeForBinding(unsigned start,
                                   unsigned end,
                                   const String& direction = "none");
  // Blink-internal version of setSelectionRange(). This translates "none"
  // direction to "forward" on platforms without "none" direction.
  // This returns true if it updated cached selection and/or FrameSelection.
  bool SetSelectionRange(
      unsigned start,
      unsigned end,
      TextFieldSelectionDirection = kSelectionHasNoDirection);
  SelectionInDOMTree Selection() const;

  int maxLength() const;
  int minLength() const;
  void setMaxLength(int, ExceptionState&);
  void setMinLength(int, ExceptionState&);

  // Dispatch 'change' event if the value is updated.
  void DispatchFormControlChangeEvent();
  // Enqueue 'change' event if the value is updated.
  void EnqueueChangeEvent();
  // This should be called on every user-input, before the user-input changes
  // the value.
  void SetValueBeforeFirstUserEditIfNotSet();
  // This should be called on every user-input, after the user-input changed the
  // value. The argument is the updated value.
  void CheckIfValueWasReverted(const String&);
  void ClearValueBeforeFirstUserEdit();

  virtual String Value() const = 0;
  virtual void SetValue(
      const String&,
      TextFieldEventBehavior = TextFieldEventBehavior::kDispatchNoEvent,
      TextControlSetValueSelection =
          TextControlSetValueSelection::kSetSelectionToEnd,
      WebAutofillState = WebAutofillState::kNotFilled) = 0;

  TextControlInnerEditorElement* InnerEditorElement() const {
    return inner_editor_.Get();
  }
  HTMLElement* CreateInnerEditorElement();
  void DropInnerEditorElement() { inner_editor_ = nullptr; }

  void SelectionChanged(bool user_triggered);
  bool LastChangeWasUserEdit() const;

  virtual void SetInnerEditorValue(const String&);
  static void AppendTextOrBr(const String& value, ContainerNode& container);
  // Returns the user-visible editing text.
  // This cost should be O(1), and may be faster than
  // SerializeInnerEdtitorValue().
  virtual String InnerEditorValue() const;
  // Serialize the user-visible editing text.
  // This cost might be O(N) where N is the number of InnerEditor children.
  String SerializeInnerEditorValue() const;
  // Returns the length of the user-visible editing text, and its is_8bit flag
  // without serializing the text. `offset_map` can be nullptr.
  std::pair<wtf_size_t, bool> AnalyzeInnerEditorValue(
      HeapHashMap<Member<const Text>, unsigned>* offset_map) const;
  // Returns a selection index value for the specified position.
  unsigned IndexForPosition(const Position& editor_position) const;

  Node* CreatePlaceholderBreakElement() const;
  // Returns true if the specified node was created by
  // CreatePlaceholderBreakElement().
  static bool IsPlaceholderBreakElement(const Node* node);

  String DirectionForFormData() const;
  // https://html.spec.whatwg.org/#auto-directionality-form-associated-elements
  // Check if, when dir=auto, we should use the value to define text direction.
  // For example, when value contains a bidirectional character.
  virtual bool IsAutoDirectionalityFormAssociated() const = 0;

  // Set the value trimmed to the max length of the field and dispatch the input
  // and change events. If |value| is empty, the autofill state is always
  // set to WebAutofillState::kNotFilled.
  void SetAutofillValue(const String& value,
                        WebAutofillState = WebAutofillState::kAutofilled);

  // A null value indicates that the suggested value should be hidden.
  virtual void SetSuggestedValue(const String& value);
  const String& SuggestedValue() const;

  void ScheduleSelectionchangeEvent();

  void ResetEventQueueStatus(const AtomicString& event_type) override {
    if (event_type == event_type_names::kSelectionchange)
      has_scheduled_selectionchange_event_ = false;
  }

  void Trace(Visitor*) const override;

  TextOverflowData ValueForTextOverflow() const;

  // Register/unregister ranges that need to be notified of value changes.
  void RegisterFormControlRange(FormControlRange* range);
  void UnregisterFormControlRange(FormControlRange* range);

  // Use the pre-edit baseline to compute and apply the edit once an observable
  // value mutation occurs, before 'input' listeners run.
  void CommitFormControlRangeEdit();

  // Handles programmatic value changes by diffing the previous contents against
  // the current InnerEditorValue(), using a selection-bounded replace model.
  // Used when edits bypass 'beforeinput'.
  void CommitProgrammaticFormControlRangeEdit(const String& old_value,
                                              unsigned old_sel_start,
                                              unsigned old_sel_end);

  // Update FormControlRanges by diffing the old and current values,
  // constrained to the original selection range.
  void ApplyFormControlRangeUpdate(const String& old_value,
                                   unsigned old_sel_start,
                                   unsigned old_sel_end);

  // Controls whether the next SetValue() call skips its automatic
  // FormControlRange update. When true, the default full-value diff is
  // suppressed so callers (e.g. setRangeText) can perform their own targeted
  // update. Cleared immediately after that SetValue() call.
  void SetSkipNextSetValueAutoDiff(bool should_skip);

  // Returns whether the next SetValue() call should skip its automatic
  // FormControlRange update.
  bool ShouldSkipNextSetValueAutoDiff() const;

 protected:
  TextControlElement(const QualifiedName&, Document&);
  virtual HTMLElement* UpdatePlaceholderText() = 0;

  // Creates the editor if necessary. Implementations that support an editor
  // should callback to CreateInnerEditorElement().
  virtual void CreateInnerEditorElementIfNecessary() const = 0;

  void ParseAttribute(const AttributeModificationParams&) override;

  void RestoreCachedSelection();

  void DefaultEventHandler(Event&) override;
  virtual void SubtreeHasChanged() = 0;

  void SetLastChangeWasNotUserEdit() { last_change_was_user_edit_ = false; }
  void AdjustPlaceholderBreakElement();
  String ValueWithHardLineBreaks() const;

  void CloneNonAttributePropertiesFrom(const Element&,
                                       NodeCloningData&) override;

  // Returns the value string. `length` and `is_8bit` must be computed by
  // AnalyzeInnerEditorValue().
  String SerializeInnerEditorValueInternal(wtf_size_t length,
                                           bool is_8bit) const;
  // Returns true if the inner-editor value is empty. This may be cheaper
  // than calling InnerEditorValue(), and InnerEditorValue() returns
  // the wrong thing if the editor hasn't been created yet.
  virtual bool IsInnerEditorValueEmpty() const = 0;

  TextControlInnerEditorElement* EnsureInnerEditorElement() const {
    if (!inner_editor_) {
      CreateInnerEditorElementIfNecessary();
    }
    return inner_editor_.Get();
  }

 private:
  // Used by ComputeSelection() to specify which values are needed.
  enum ComputeSelectionFlags {
    kStart = 1 << 0,
    kEnd = 1 << 1,
    kDirection = 1 << 2,
  };

  struct ComputedSelection {
    unsigned start = 0;
    unsigned end = 0;
    TextFieldSelectionDirection direction = kSelectionHasNoDirection;
  };

  bool ShouldApplySelectionCache() const;
  // Computes the selection. `flags` is a bitmask of ComputeSelectionFlags that
  // indicates what values should be computed.
  void ComputeSelection(uint32_t flags,
                        ComputedSelection& computed_selection) const;
  // Returns true if cached values and arguments are not same.
  bool CacheSelection(unsigned start,
                      unsigned end,
                      TextFieldSelectionDirection);
  static unsigned IndexForPosition(HTMLElement* inner_editor, const Position&);

  bool DispatchFocusEvent(Element* old_focused_element,
                          mojom::blink::FocusType,
                          InputDeviceCapabilities* source_capabilities) final;
  void DispatchBlurEvent(Element* new_focused_element,
                         mojom::blink::FocusType,
                         InputDeviceCapabilities* source_capabilities) final;
  void ScheduleSelectEvent();
  void ScheduleSelectionchangeEventOnThisOrDocument();
  void DisabledOrReadonlyAttributeChanged(const QualifiedName&);

  // Called in dispatchFocusEvent(), after placeholder process, before calling
  // parent's dispatchFocusEvent().
  virtual void HandleFocusEvent(Element* /* oldFocusedNode */,
                                mojom::blink::FocusType) {}
  // Called in dispatchBlurEvent(), after placeholder process, before calling
  // parent's dispatchBlurEvent().
  virtual void HandleBlurEvent() {}

  // Whether the placeholder attribute value should be visible. Does not
  // necessarily match the placeholder_element visibility because it can be used
  // for suggested values too.
  bool PlaceholderShouldBeVisible() const;

  // Notify observers of a single replace in this element’s value at
  // `change_offset`: removed `deleted_count` and added `inserted_count`
  // characters.
  void NotifyFormControlRangesOfTextChange(unsigned change_offset,
                                           unsigned deleted_count,
                                           unsigned inserted_count) const;

  // Capture the control’s pre-edit value and selection at 'beforeinput'.
  // This baseline is held until the first observable value mutation.
  void CaptureFormControlRangePreEdit();

  // Held directly instead of looked up by ID for speed.
  // Not only is the lookup faster, but for simple text inputs it avoids
  // creating a number of TreeScope data structures to track elements by ID.
  Member<TextControlInnerEditorElement> inner_editor_;

  // In value_before_first_user_edit_, we distinguish a null String and
  // zero-length String. Null String means the field doesn't have any data yet,
  // and zero-length String is a valid data.
  String value_before_first_user_edit_;
  bool last_change_was_user_edit_;

  unsigned cached_selection_start_;
  unsigned cached_selection_end_;
  TextFieldSelectionDirection cached_selection_direction_;

  String suggested_value_;
  String value_before_set_suggested_value_;

  // Snapshot taken at 'beforeinput' retained until the first observable change.
  // Selection defines the edit region; that change is treated as one replace
  // of the region (e.g. replacing "abc" with "xyz") rather than multiple
  // small diffs, so ranges update consistently.
  struct PendingUserEditSnapshot {
    String old_value;
    unsigned selection_start = 0;
    unsigned selection_end = 0;
  };

  // Holds a pending user edit captured at 'beforeinput' until the first
  // observable value mutation occurs.
  std::optional<PendingUserEditSnapshot> pending_user_edit_;

  // Holds FormControlRange instances that observe this text control.
  HeapVector<Member<FormControlRange>> form_control_ranges_;

  // RAII helper that temporarily skips SetValue()’s automatic FormControlRange
  // full-value diff for this scope. The flag is restored on destruction.
  class ScopedSkipValueAutoDiff final {
   public:
    explicit ScopedSkipValueAutoDiff(TextControlElement& element)
        : auto_reset_(&element.skip_next_set_value_auto_diff_, true) {}
    ScopedSkipValueAutoDiff(const ScopedSkipValueAutoDiff&) = delete;
    ScopedSkipValueAutoDiff& operator=(const ScopedSkipValueAutoDiff&) = delete;

   private:
    base::AutoReset<bool> auto_reset_;
  };

  // Skip SetValue's automatic FormControlRange full-value diff on the next
  // call. Used by setRangeText(), which issues its own precise, range-scoped
  // update. Cleared immediately after that SetValue() call.
  bool skip_next_set_value_auto_diff_ = false;

  // Indicate whether there is one scheduled selectionchange event.
  bool has_scheduled_selectionchange_event_ = false;

  FRIEND_TEST_ALL_PREFIXES(TextControlElementTest, IndexForPosition);
  FRIEND_TEST_ALL_PREFIXES(HTMLTextAreaElementTest, ValueWithHardLineBreaks);
  FRIEND_TEST_ALL_PREFIXES(HTMLTextAreaElementTest, ValueWithHardLineBreaksRtl);
};

inline bool IsTextControl(const Node& node) {
  auto* element = DynamicTo<Element>(node);
  return element && element->IsTextControl();
}
inline bool IsTextControl(const Node* node) {
  return node && IsTextControl(*node);
}

// ToTextControl() is stricter than To<TextControlElement>(). ToTextControl()
// does not accept HTMLInputElement with non-text types.
#define DEFINE_TEXT_CONTROL_CASTS(Type, ArgType)                              \
  inline Type* ToTextControl(ArgType* node) {                                 \
    DCHECK(!node || IsTextControl(*node));                                    \
    return To<TextControlElement>(node);                                      \
  }                                                                           \
  inline Type& ToTextControl(ArgType& node) {                                 \
    DCHECK(IsTextControl(node));                                              \
    return To<TextControlElement>(node);                                      \
  }                                                                           \
  inline Type* ToTextControlOrNull(ArgType* node) {                           \
    return node && IsTextControl(*node) ? static_cast<Type*>(node) : nullptr; \
  }                                                                           \
  inline Type* ToTextControlOrNull(ArgType& node) {                           \
    return IsTextControl(node) ? static_cast<Type*>(&node) : nullptr;         \
  }                                                                           \
  void ToTextControl(Type*);                                                  \
  void ToTextControl(Type&)

DEFINE_TEXT_CONTROL_CASTS(TextControlElement, Node);
DEFINE_TEXT_CONTROL_CASTS(const TextControlElement, const Node);

#undef DEFINE_TEXT_CONTROL_CASTS

template <>
struct DowncastTraits<TextControlElement> {
  static bool AllowFrom(const Node& node) {
    return node.HasTagName(html_names::kInputTag) ||
           node.HasTagName(html_names::kTextareaTag);
  }
};

TextControlElement* EnclosingTextControl(const Position&);
TextControlElement* EnclosingTextControl(const PositionInFlatTree&);
CORE_EXPORT TextControlElement* EnclosingTextControl(const Node*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_TEXT_CONTROL_ELEMENT_H_
