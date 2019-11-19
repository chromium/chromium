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

#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"

namespace blink {

namespace {

enum class AutoCompleteCategory {
  kNone,
  kOff,
  kAutomatic,
  kNormal,
  kContact,
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
      }));

  auto iter = category_map.find(token);
  return iter == category_map.end() ? AutoCompleteCategory::kNone : iter->value;
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
  return FormControlType() == input_type_names::kHidden;
}

String HTMLFormControlElementWithState::IDLExposedAutofillValue() const {
  // TODO(tkent): Share the code with autofill::FormStructure::
  // ParseFieldTypesFromAutocompleteAttributes().

  // https://html.spec.whatwg.org/C/#autofill-processing-model
  // 1. If the element has no autocomplete attribute, then jump to the step
  // labeled default.
  const AtomicString& value = FastGetAttribute(html_names::kAutocompleteAttr);
  if (value.IsNull())
    return g_empty_string;

  // 2. Let tokens be the result of splitting the attribute's value on ASCII
  // whitespace.
  SpaceSplitString tokens(value.LowerASCII());
  if (tokens.size() == 0)
    return g_empty_string;

  // 4. Let index be the index of the last token in tokens.
  wtf_size_t index = tokens.size() - 1;

  // 5. If the indexth token in tokens is not an ASCII case-insensitive
  // match for one of the tokens given in the first column of the
  // following table, or if the number of tokens in tokens is greater
  // than the maximum number given in the cell in the second column of
  // that token's row, then jump to the step labeled default. Otherwise,
  // let field be the string given in the cell of the first column of
  // the matching row, and let category be the value of the cell in the
  // third column of that same row.
  AtomicString token = tokens[index];
  AutoCompleteCategory category = GetAutoCompleteCategory(token);
  wtf_size_t max_tokens;
  switch (category) {
    case AutoCompleteCategory::kNone:
      max_tokens = 0;
      break;
    case AutoCompleteCategory::kOff:
    case AutoCompleteCategory::kAutomatic:
      max_tokens = 1;
      break;
    case AutoCompleteCategory::kNormal:
      max_tokens = 3;
      break;
    case AutoCompleteCategory::kContact:
      max_tokens = 4;
      break;
  }
  if (tokens.size() > max_tokens)
    return g_empty_string;
  AtomicString field = token;

  // 6. If category is Off or Automatic but the element's autocomplete attribute
  // is wearing the autofill anchor mantle, then jump to the step labeled
  // default.
  if ((category == AutoCompleteCategory::kOff ||
       category == AutoCompleteCategory::kAutomatic) &&
      IsWearingAutofillAnchorMantle()) {
    return g_empty_string;
  }

  // 7. If category is Off, let the element's autofill field name be the string
  // "off", let its autofill hint set be empty, and let its IDL-exposed autofill
  // value be the string "off". Then, return.
  if (category == AutoCompleteCategory::kOff)
    return "off";

  // 8. If category is Automatic, let the element's autofill field name be the
  // string "on", let its autofill hint set be empty, and let its IDL-exposed
  // autofill value be the string "on". Then, return.
  if (category == AutoCompleteCategory::kAutomatic)
    return "on";

  // 11. Let IDL value have the same value as field.
  String idl_value = field;
  // 12. If the indexth token in tokens is the first entry, then skip to the
  // step labeled done.
  if (index != 0) {
    // 13. Decrement index by one.
    --index;
    // 14. If category is Contact and the indexth token in tokens is an ASCII
    // case-insensitive match for one of the strings in the following list, ...
    if (category == AutoCompleteCategory::kContact) {
      AtomicString contact = tokens[index];
      if (contact == "home" || contact == "work" || contact == "mobile" ||
          contact == "fax" || contact == "pager") {
        // 14.4. Let IDL value be the concatenation of contact, a U+0020 SPACE
        // character, and the previous value of IDL value (which at this point
        // will always be field).
        idl_value = contact + " " + idl_value;
        // 14.5. If the indexth entry in tokens is the first entry, then skip to
        // the step labeled done.
        if (index == 0) {
          return idl_value;
        }
        // 14.6. Decrement index by one.
        --index;
      }
    }

    // 15. If the indexth token in tokens is an ASCII case-insensitive match for
    // one of the strings in the following list, ...
    AtomicString mode = tokens[index];
    if (mode == "shipping" || mode == "billing") {
      // 15.4. Let IDL value be the concatenation of mode, a U+0020 SPACE
      // character, and the previous value of IDL value (which at this point
      // will either be field or the concatenation of contact, a space, and
      // field).
      idl_value = mode + " " + idl_value;
      // 15.5 If the indexth entry in tokens is the first entry, then skip to
      // the step labeled done.
      if (index == 0) {
        return idl_value;
      }
      // 15.6. Decrement index by one.
      --index;
    }

    // 16. If the indexth entry in tokens is not the first entry, then jump to
    // the step labeled default.
    if (index != 0)
      return g_empty_string;
    // 17. If the first eight characters of the indexth token in tokens are not
    // an ASCII case-insensitive match for the string "section-", then jump to
    // the step labeled default.
    AtomicString section = tokens[index];
    if (!section.StartsWith("section-"))
      return g_empty_string;
    // 20. Let IDL value be the concatenation of section, a U+0020 SPACE
    // character, and the previous value of IDL value.
    idl_value = section + " " + idl_value;
  }
  // 24. Let the element's IDL-exposed autofill value be IDL value.
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
  // We don't save/restore control state in a form with autocomplete=off.
  return isConnected() && ShouldAutocomplete();
}

void HTMLFormControlElementWithState::FinishParsingChildren() {
  HTMLFormControlElement::FinishParsingChildren();
  ListedElement::TakeStateAndRestore();
}

bool HTMLFormControlElementWithState::IsFormControlElementWithState() const {
  return true;
}

int HTMLFormControlElementWithState::DefaultTabIndex() const {
  return 0;
}

}  // namespace blink
