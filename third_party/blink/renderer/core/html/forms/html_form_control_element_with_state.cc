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

#include "third_party/blink/renderer/core/html/forms/html_form_control_element_with_state.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

enum class AutoCompleteCategory {
  kNone,
  kOff,
  kAutomatic,
  kNormal,
  kContact,
  kCredential,
};

AutoCompleteCategory GetAutoCompleteCategory(const AtomicString& token) {
  using Map = HashMap<AtomicString, AutoCompleteCategory>;
  DEFINE_STATIC_LOCAL(
      Map, category_map,
      ({
          {"off", AutoCompleteCategory::kOff},
          {"on", AutoCompleteCategory::kAutomatic},

          {"name", AutoCompleteCategory::kNormal},
          {"honorific-prefix", AutoCompleteCategory::kNormal},
          {"given-name", AutoCompleteCategory::kNormal},
          {"additional-name", AutoCompleteCategory::kNormal},
          {"family-name", AutoCompleteCategory::kNormal},
          {"honorific-suffix", AutoCompleteCategory::kNormal},
          {"nickname", AutoCompleteCategory::kNormal},
          {"organization-title", AutoCompleteCategory::kNormal},
          {"username", AutoCompleteCategory::kNormal},
          {"new-password", AutoCompleteCategory::kNormal},
          {"current-password", AutoCompleteCategory::kNormal},
          {"one-time-code", AutoCompleteCategory::kNormal},
          {"organization", AutoCompleteCategory::kNormal},
          {"street-address", AutoCompleteCategory::kNormal},
          {"address-line1", AutoCompleteCategory::kNormal},
          {"address-line2", AutoCompleteCategory::kNormal},
          {"address-line3", AutoCompleteCategory::kNormal},
          {"address-level4", AutoCompleteCategory::kNormal},
          {"address-level3", AutoCompleteCategory::kNormal},
          {"address-level2", AutoCompleteCategory::kNormal},
          {"address-level1", AutoCompleteCategory::kNormal},
          {"country", AutoCompleteCategory::kNormal},
          {"country-name", AutoCompleteCategory::kNormal},
          {"postal-code", AutoCompleteCategory::kNormal},
          {"cc-name", AutoCompleteCategory::kNormal},
          {"cc-given-name", AutoCompleteCategory::kNormal},
          {"cc-additional-name", AutoCompleteCategory::kNormal},
          {"cc-family-name", AutoCompleteCategory::kNormal},
          {"cc-number", AutoCompleteCategory::kNormal},
          {"cc-exp", AutoCompleteCategory::kNormal},
          {"cc-exp-month", AutoCompleteCategory::kNormal},
          {"cc-exp-year", AutoCompleteCategory::kNormal},
          {"cc-csc", AutoCompleteCategory::kNormal},
          {"cc-type", AutoCompleteCategory::kNormal},
          {"transaction-currency", AutoCompleteCategory::kNormal},
          {"transaction-amount", AutoCompleteCategory::kNormal},
          {"language", AutoCompleteCategory::kNormal},
          {"bday", AutoCompleteCategory::kNormal},
          {"bday-day", AutoCompleteCategory::kNormal},
          {"bday-month", AutoCompleteCategory::kNormal},
          {"bday-year", AutoCompleteCategory::kNormal},
          {"sex", AutoCompleteCategory::kNormal},
          {"url", AutoCompleteCategory::kNormal},
          {"photo", AutoCompleteCategory::kNormal},

          {"tel", AutoCompleteCategory::kContact},
          {"tel-country-code", AutoCompleteCategory::kContact},
          {"tel-national", AutoCompleteCategory::kContact},
          {"tel-area-code", AutoCompleteCategory::kContact},
          {"tel-local", AutoCompleteCategory::kContact},
          {"tel-local-prefix", AutoCompleteCategory::kContact},
          {"tel-local-suffix", AutoCompleteCategory::kContact},
          {"tel-extension", AutoCompleteCategory::kContact},
          {"email", AutoCompleteCategory::kContact},
          {"impp", AutoCompleteCategory::kContact},

          {"webauthn", AutoCompleteCategory::kCredential},
      }));

  auto iter = category_map.find(token);
  return iter == category_map.end() ? AutoCompleteCategory::kNone : iter->value;
}

wtf_size_t GetMaxTokensForCategory(AutoCompleteCategory category) {
  switch (category) {
    case AutoCompleteCategory::kNone:
      return 0;
    case AutoCompleteCategory::kOff:
    case AutoCompleteCategory::kAutomatic:
      return 1;
    case AutoCompleteCategory::kNormal:
      return 3;
    case AutoCompleteCategory::kContact:
      return 4;
    case AutoCompleteCategory::kCredential:
      return 5;
  }
}

}  // anonymous namespace

HTMLFormControlElementWithState::HTMLFormControlElementWithState(
    const QualifiedName& tag_name,
    Document& doc)
    : HTMLFormControlElement(tag_name, doc) {}

HTMLFormControlElementWithState::~HTMLFormControlElementWithState() = default;

bool HTMLFormControlElementWithState::ShouldAutocomplete() const {
  if (!Form())
    return true;
  return Form()->ShouldAutocomplete();
}

bool HTMLFormControlElementWithState::IsWearingAutofillAnchorMantle() const {
  return FormControlType() == FormControlType::kInputHidden;
}

String HTMLFormControlElementWithState::IDLExposedAutofillValue() const {
  // TODO(tkent): Share the code with `autofill::ParseAutocompleteAttribute()`.

  // https://html.spec.whatwg.org/C/#autofill-processing-model
  // 1. If the element has no autocomplete attribute, then jump to the step
  // labeled default.
  const AtomicString& value = FastGetAttribute(html_names::kAutocompleteAttr);
  if (value.IsNull())
    return g_empty_string;

  // 2. Let tokens be the result of splitting the attribute's value on ASCII
  // whitespace.
  SpaceSplitString tokens(value.LowerASCII());

  // 3. If tokens is empty, then jump to the step labeled default.
  if (tokens.size() == 0)
    return g_empty_string;

  // 4. Let index be the index of the last token in tokens.
  wtf_size_t index = tokens.size() - 1;

  // 5. Let field be the indexth token in tokens.
  AtomicString field = tokens[index];

  // 6. Let the category, maximum tokens pair be the result of executing the
  // algorithm to determine a field's category with field.
  AtomicString token = tokens[index];
  AutoCompleteCategory category = GetAutoCompleteCategory(token);
  wtf_size_t max_tokens = GetMaxTokensForCategory(category);

  // 7. If category is empty, then jump to the step labeled default.
  if (category == AutoCompleteCategory::kNone) {
    return g_empty_string;
  }

  // 8. If the number of tokens in tokens is greater than maximum tokens, then
  // jump to the step labeled default.
  if (tokens.size() > max_tokens)
    return g_empty_string;

  // 9. If category is Off or Automatic but the element's autocomplete attribute
  // is wearing the autofill anchor mantle, then jump to the step labeled
  // default.
  if ((category == AutoCompleteCategory::kOff ||
       category == AutoCompleteCategory::kAutomatic) &&
      IsWearingAutofillAnchorMantle()) {
    return g_empty_string;
  }

  // 10. If category is Off, let the element's autofill field name be the string
  // "off", let its autofill hint set be empty, and let its IDL-exposed autofill
  // value be the string "off". Then, return.
  if (category == AutoCompleteCategory::kOff)
    return "off";

  // 11. If category is Automatic, let the element's autofill field name be the
  // string "on", let its autofill hint set be empty, and let its IDL-exposed
  // autofill value be the string "on". Then, return.
  if (category == AutoCompleteCategory::kAutomatic)
    return "on";

  // 15. Let IDL value have the same value as field.
  String idl_value = field;

  // 16. If category is Credential and the indexth token in tokens is an ASCII
  // case-insensitive match for "webauthn", then run the substeps that follow:
  if (category == AutoCompleteCategory::kCredential) {
    // 16.2 If the indexth token in tokens is the first entry, then skip to the
    // step labeled done.
    if (index != 0) {
      // 16.3 Decrement index by one.
      --index;
      // 16.4 Let the category, maximum tokens pair be the result of executing
      // the algorithm to determine a field's category with the indexth token in
      // tokens.
      category = GetAutoCompleteCategory(tokens[index]);
      // 16.5 If category is not Normal and category is not Contact, then jump
      // to the step labeled default.
      if (category != AutoCompleteCategory::kNormal &&
          category != AutoCompleteCategory::kContact) {
        return g_empty_string;
      }
      // 16.6 If index is greater than maximum tokens minus one (i.e. if the
      // number of remaining tokens is greater than maximum tokens), then jump
      // to the step labeled default.
      if (index > GetMaxTokensForCategory(category) - 1) {
        return g_empty_string;
      }
      // 16.7 Let IDL value be the concatenation of the indexth token in tokens,
      // a U+0020 SPACE character, and the previous value of IDL value.
      idl_value = tokens[index] + " " + idl_value;
    }
  }

  // 17. If the indexth token in tokens is the first entry, then skip to the
  // step labeled done.
  if (index != 0) {
    // 18. Decrement index by one.
    --index;
    // 19. If category is Contact and the indexth token in tokens is an ASCII
    // case-insensitive match for one of the strings in the following list, ...
    if (category == AutoCompleteCategory::kContact) {
      AtomicString contact = tokens[index];
      if (contact == "home" || contact == "work" || contact == "mobile" ||
          contact == "fax" || contact == "pager") {
        // 19.4. Let IDL value be the concatenation of contact, a U+0020 SPACE
        // character, and the previous value of IDL value (which at this point
        // will always be field).
        idl_value = contact + " " + idl_value;
        // 19.5. If the indexth entry in tokens is the first entry, then skip to
        // the step labeled done.
        if (index == 0) {
          return idl_value;
        }
        // 19.6. Decrement index by one.
        --index;
      }
    }

    // 20. If the indexth token in tokens is an ASCII case-insensitive match for
    // one of the strings in the following list, ...
    AtomicString mode = tokens[index];
    if (mode == "shipping" || mode == "billing") {
      // 20.4. Let IDL value be the concatenation of mode, a U+0020 SPACE
      // character, and the previous value of IDL value (which at this point
      // will either be field or the concatenation of contact, a space, and
      // field).
      idl_value = mode + " " + idl_value;
      // 20.5 If the indexth entry in tokens is the first entry, then skip to
      // the step labeled done.
      if (index == 0) {
        return idl_value;
      }
      // 20.6. Decrement index by one.
      --index;
    }

    // 21. If the indexth entry in tokens is not the first entry, then jump to
    // the step labeled default.
    if (index != 0)
      return g_empty_string;
    // 22. If the first eight characters of the indexth token in tokens are not
    // an ASCII case-insensitive match for the string "section-", then jump to
    // the step labeled default.
    AtomicString section = tokens[index];
    if (!section.StartsWith("section-"))
      return g_empty_string;
    // 25. Let IDL value be the concatenation of section, a U+0020 SPACE
    // character, and the previous value of IDL value.
    idl_value = section + " " + idl_value;
  }
  // 30. Let the element's IDL-exposed autofill value be IDL value.
  return idl_value;
}

void HTMLFormControlElementWithState::setIDLExposedAutofillValue(
    const String& autocomplete_value) {
  setAttribute(html_names::kAutocompleteAttr, AtomicString(autocomplete_value));
}

bool HTMLFormControlElementWithState::ClassSupportsStateRestore() const {
  return true;
}

bool HTMLFormControlElementWithState::ShouldSaveAndRestoreFormControlState()
    const {
  if (!isConnected()) {
    return false;
  }
  // TODO(crbug.com/1419161): remove this after M113 has been stable for a bit.
  if (RuntimeEnabledFeatures::
          FormControlRestoreStateIfAutocompleteOffEnabled()) {
    return ShouldAutocomplete();
  }
  if (Form() && !Form()->ShouldAutocomplete()) {
    return false;
  }
  if (EqualIgnoringASCIICase(FastGetAttribute(html_names::kAutocompleteAttr),
                             "off")) {
    return false;
  }
  return true;
}

void HTMLFormControlElementWithState::DispatchInputEvent() {
  // Legacy 'input' event for forms set value and checked.
  Event* event = Event::CreateBubble(event_type_names::kInput);
  event->SetComposed(true);
  DispatchScopedEvent(*event);
}

void HTMLFormControlElementWithState::DispatchChangeEvent() {
  if (UserHasEditedTheField()) {
    // Start matching :user-valid, but only if the user has already edited the
    // field.
    SetUserHasEditedTheFieldAndBlurred();
  }
  DispatchScopedEvent(*Event::CreateBubble(event_type_names::kChange));
}

void HTMLFormControlElementWithState::DispatchCancelEvent() {
  DispatchScopedEvent(*Event::CreateBubble(event_type_names::kCancel));
}

void HTMLFormControlElementWithState::FinishParsingChildren() {
  HTMLFormControlElement::FinishParsingChildren();
  ListedElement::TakeStateAndRestore();
}

bool HTMLFormControlElementWithState::IsFormControlElementWithState() const {
  return true;
}

void HTMLFormControlElementWithState::ResetImpl() {
  ClearUserHasEditedTheField();
}

int HTMLFormControlElementWithState::DefaultTabIndex() const {
  return 0;
}

void HTMLFormControlElementWithState::SetUserHasEditedTheField() {
  if (interacted_state_ < InteractedState::kInteractedAndStillFocused) {
    interacted_state_ = InteractedState::kInteractedAndStillFocused;
  }
}

void HTMLFormControlElementWithState::SetUserHasEditedTheFieldAndBlurred() {
  if (interacted_state_ >= InteractedState::kInteractedAndBlurred) {
    return;
  }
  interacted_state_ = InteractedState::kInteractedAndBlurred;
  PseudoStateChanged(CSSSelector::kPseudoUserInvalid);
  PseudoStateChanged(CSSSelector::kPseudoUserValid);
}

void HTMLFormControlElementWithState::ForceUserValid() {
  force_user_valid_ = true;
  PseudoStateChanged(CSSSelector::kPseudoUserInvalid);
  PseudoStateChanged(CSSSelector::kPseudoUserValid);
}

bool HTMLFormControlElementWithState::MatchesUserInvalidPseudo() {
  return (UserHasEditedTheFieldAndBlurred() || force_user_valid_) &&
         MatchesValidityPseudoClasses() && !ListedElement::IsValidElement();
}

bool HTMLFormControlElementWithState::MatchesUserValidPseudo() {
  return (UserHasEditedTheFieldAndBlurred() || force_user_valid_) &&
         MatchesValidityPseudoClasses() && ListedElement::IsValidElement();
}

}  // namespace blink
