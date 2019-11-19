/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_TEXT_AREA_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_TEXT_AREA_ELEMENT_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

namespace blink {

class BeforeTextInsertedEvent;

class CORE_EXPORT HTMLTextAreaElement final : public TextControlElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLTextAreaElement(Document&);

  unsigned cols() const { return cols_; }
  unsigned rows() const { return rows_; }

  bool ShouldWrapText() const { return wrap_ != kNoWrap; }

  String value() const override;
  void setValue(
      const String&,
      TextFieldEventBehavior = TextFieldEventBehavior::kDispatchNoEvent,
      TextControlSetValueSelection =
          TextControlSetValueSelection::kSetSelectionToEnd) override;
  String defaultValue() const;
  void setDefaultValue(const String&);
  int textLength() const { return value().length(); }

  void SetSuggestedValue(const String& value) override;

  // For ValidityState
  String validationMessage() const override;
  bool ValueMissing() const override;
  bool TooLong() const override;
  bool TooShort() const override;
  bool IsValidValue(const String&) const;

  void setCols(unsigned);
  void setRows(unsigned);

 private:
  FRIEND_TEST_ALL_PREFIXES(HTMLTextAreaElementTest, SanitizeUserInputValue);

  enum WrapMethod { kNoWrap, kSoftWrap, kHardWrap };

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  // FIXME: Author shadows should be allowed
  // https://bugs.webkit.org/show_bug.cgi?id=92608
  bool AreAuthorShadowsAllowed() const override { return false; }

  void HandleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) const;
  static String SanitizeUserInputValue(const String&, unsigned max_length);
  void UpdateValue();
  void SetNonDirtyValue(const String&, TextControlSetValueSelection);
  void SetValueCommon(const String&,
                      TextFieldEventBehavior,
                      TextControlSetValueSelection);

  bool IsPlaceholderVisible() const override { return is_placeholder_visible_; }
  void SetPlaceholderVisibility(bool) override;
  bool SupportsPlaceholder() const override { return true; }
  String GetPlaceholderValue() const final;
  void UpdatePlaceholderText() override;
  bool IsEmptyValue() const override { return value().IsEmpty(); }

  bool IsOptionalFormControl() const override {
    return !IsRequiredFormControl();
  }
  bool IsRequiredFormControl() const override { return IsRequired(); }

  void DefaultEventHandler(Event&) override;

  void SubtreeHasChanged() override;

  bool IsEnumeratable() const override { return true; }
  bool IsInteractiveContent() const override;
  bool IsLabelable() const override { return true; }

  const AtomicString& FormControlType() const override;

  FormControlState SaveFormControlState() const override;
  void RestoreFormControlState(const FormControlState&) override;

  bool IsTextControl() const override { return true; }

  void ChildrenChanged(const ChildrenChange&) override;
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  void AppendToFormData(FormData&) override;
  void ResetImpl() override;
  bool HasCustomFocusLogic() const override;
  bool MayTriggerVirtualKeyboard() const override;
  bool IsKeyboardFocusable() const override;
  void UpdateFocusAppearanceWithOptions(SelectionBehaviorOnFocus,
                                        const FocusOptions*) override;

  void AccessKeyAction(bool send_mouse_events) override;

  bool MatchesReadOnlyPseudoClass() const override;
  bool MatchesReadWritePseudoClass() const override;
  void CloneNonAttributePropertiesFrom(const Element&, CloneChildrenFlag) final;

  // If the String* argument is 0, apply value().
  bool ValueMissing(const String*) const;
  bool TooLong(const String*, NeedsToCheckDirtyFlag) const;
  bool TooShort(const String*, NeedsToCheckDirtyFlag) const;

  unsigned rows_;
  unsigned cols_;
  WrapMethod wrap_;
  mutable String value_;
  mutable bool is_dirty_;
  unsigned is_placeholder_visible_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_TEXT_AREA_ELEMENT_H_
