/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FORM_CONTROL_ELEMENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FORM_CONTROL_ELEMENT_H_

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace blink {

class HTMLFormControlElement;

// Provides readonly access to some properties of a DOM form control element
// node.
class BLINK_EXPORT WebFormControlElement : public WebElement {
 public:
  WebFormControlElement() : WebElement() {}
  WebFormControlElement(const WebFormControlElement& e) = default;

  WebFormControlElement& operator=(const WebFormControlElement& e) {
    WebElement::Assign(e);
    return *this;
  }
  void Assign(const WebFormControlElement& e) { WebElement::Assign(e); }

  bool IsEnabled() const;
  bool IsReadOnly() const;
  WebString FormControlName() const;
  WebString FormControlType() const;

  // Same as FormControlType() but returns the type "password" for text fields
  // that have been a password in the past.
  WebString FormControlTypeForAutofill() const;

  enum WebAutofillState GetAutofillState() const;
  bool IsAutofilled() const;
  void SetAutofillState(enum WebAutofillState);
  bool UserHasEditedTheField() const;
  // This is only used for simulating the user's action in tests.
  void SetUserHasEditedTheFieldForTest();

  // The autofill section to which this element belongs (e.g. billing address,
  // shipping address, .. .)
  WebString AutofillSection() const;
  void SetAutofillSection(const WebString&);

  // Returns true if autocomplete attribute of the element is not set as "off".
  bool AutoComplete() const;

  // Sets value for input element, textarea element and select element. For
  // select element it finds the option with value matches the given parameter
  // and make the option as the current selection.
  void SetValue(const WebString&, bool send_events = false);
  // Sets the autofilled value for input element, textarea element and select
  // element and sends a sequence of events to the element.
  void SetAutofillValue(const WebString&);
  // Returns value of element. For select element, it returns the value of
  // the selected option if present. If no selected option, an empty string
  // is returned. If element doesn't fall into input element, textarea element
  // and select element categories, a null string is returned.
  WebString Value() const;
  // Sets suggested value for element. For select element it finds the option
  // with value matches the given parameter and make the option as the suggested
  // selection. The goal of introducing suggested value is to not leak any
  // information to JavaScript.
  void SetSuggestedValue(const WebString&);
  // Returns suggested value of element. If element doesn't fall into input
  // element, textarea element and select element categories, a null string is
  // returned.
  WebString SuggestedValue() const;

  // Returns the non-sanitized, exact value inside the text input field
  // or insisde the textarea. If neither input element nor textarea element,
  // a null string is returned.
  WebString EditingValue() const;

  // Sets character selection range.
  void SetSelectionRange(int start, int end);
  // Returned value represents a cursor/caret position at the current
  // selection's start for text input field or textarea. If neither input
  // element nor textarea element, 0 is returned.
  int SelectionStart() const;
  // Returned value represents a cursor/caret position at the current
  // selection's end for text input field or textarea. If neither input
  // element nor textarea element, 0 is returned.
  int SelectionEnd() const;

  // Returns text-align(only left and right are supported. see crbug.com/482339)
  // of text of element.
  WebString AlignmentForFormData() const;

  // Returns direction of text of element.
  WebString DirectionForFormData() const;

  // Returns the name that should be used for the specified |element| when
  // storing autofill data.  This is either the field name or its id, an empty
  // string if it has no name and no id.
  WebString NameForAutofill() const;

  WebFormElement Form() const;

  // Returns the identifier which is unique among all form control elements in
  // the current renderer process. In the current implementation ids are
  // consecutive numbers so their uniqueness might be broken in case of
  // overflow.
  unsigned UniqueRendererFormControlId() const;

  // Returns the ax node id of the form control element in the accessibility
  // tree. The ax node id is consistent across renderer and browser processes.
  int32_t GetAxId() const;

#if INSIDE_BLINK
  WebFormControlElement(HTMLFormControlElement*);
  WebFormControlElement& operator=(HTMLFormControlElement*);
  operator HTMLFormControlElement*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebFormControlElement);

}  // namespace blink

#endif
