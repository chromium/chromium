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

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/i18n/rtl.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class HTMLFormControlElement;

// Provides readonly access to some properties of a DOM form control element
// node.
class BLINK_EXPORT WebFormControlElement : public WebElement {
 public:
  struct GlyphInfo {
    uint16_t glyph;
    gfx::Vector2dF offset;
    float total_advance;
  };

  struct TypefaceRunInfo {
    sk_sp<SkTypeface> typeface;
    std::vector<GlyphInfo> glyphs;
    bool is_horizontal;
  };

  struct TextRunInfo {
    std::vector<TypefaceRunInfo> typeface_runs;
    gfx::RectF location;
  };

  struct TextInfo {
    std::vector<TextRunInfo> text_runs;
    float effective_zoom;
  };

  explicit WebFormControlElement(
      cppgc::SourceLocation loc = BLINK_WEB_NODE_LOCATION_FROM_HERE)
      : WebElement(loc) {}
  WebFormControlElement(const WebFormControlElement& e) = default;

  WebFormControlElement& operator=(const WebFormControlElement& e) {
    WebElement::Assign(e);
    return *this;
  }
  void Assign(const WebFormControlElement& e) { WebElement::Assign(e); }

  bool IsEnabled() const;
  bool IsReadOnly() const;
  WebString FormControlName() const;

  mojom::FormControlType FormControlType() const;
  mojom::FormControlType FormControlTypeForAutofill() const;

  enum WebAutofillState GetAutofillState() const;
  bool IsAutofilled() const;
  bool IsPreviewed() const;
  void SetAutofillState(enum WebAutofillState);
  bool UserHasEditedTheField() const;
  void SetUserHasEditedTheField(bool value);

  // Returns true if autocomplete attribute of the element is not set as "off".
  bool AutoComplete() const;

  // Sets value for input element, textarea element and select element. For
  // select element it finds the option with value matches the given parameter
  // and make the option as the current selection.
  void SetValue(const WebString&, bool send_events = false);
  // Sets the value of a form element with `value` and updates the
  // `WebAutofillState` of the field accordingly.
  //
  // Also simulates a user's keyboard interaction:
  // - Input/TextArea: (Focus -> KeyDown -> Value Change -> KeyUp -> Blur).
  // - Select:         (Focus            -> Value Change          -> Blur).
  //
  // NOTE: For WebSelectElement, this will search for the FIRST option matching
  // `value` in the element's list of options and select it if found. Otherwise,
  // the value/state are left unchanged and no "Value Change" event is
  // dispatched.
  void SetAutofillValue(
      const WebString& value,
      WebAutofillState autofill_state = WebAutofillState::kAutofilled);
  // Triggers the emission of a focus event.
  void DispatchFocusEvent();
  // Triggers the emission of a blur event.
  void DispatchBlurEvent();
  // Returns value of element. For select element, it returns the value of
  // the selected option if present. If no selected option, an empty string
  // is returned. If element doesn't fall into input element, textarea element
  // and select element categories, a null string is returned.
  WebString Value() const;
  // Sets suggested value for element. For select element it finds the FIRST
  // option with value matches the given parameter and make the option as the
  // suggested selection.
  // A null value indicates that the suggested value should be hidden.
  void SetSuggestedValue(const WebString&);
  // Returns suggested value of element. If element doesn't fall into input
  // element, textarea element and select element categories, a null string is
  // returned.
  WebString SuggestedValue() const;

  // Returns the non-sanitized, exact value inside the text input field
  // or inside the textarea. If neither input element nor textarea element, a
  // null string is returned.
  WebString EditingValue() const;

  // The maximum length in terms of text length the form control can hold. Like
  // the maxLength IDL attribute, this is non-negative with two exceptions: if
  // the attribute does not apply to the element or the element has no (valid)
  // maximum length set, it is -1.
  int MaxLength() const;

  // Sets character selection range.
  void SetSelectionRange(unsigned start, unsigned end);
  // Returned value represents a cursor/caret position at the current
  // selection's start for text input field or textarea. If neither input
  // element nor textarea element, 0 is returned.
  unsigned SelectionStart() const;
  // Returned value represents a cursor/caret position at the current
  // selection's end for text input field or textarea. If neither input
  // element nor textarea element, 0 is returned.
  unsigned SelectionEnd() const;

  // The text align values.
  enum class Alignment { kNotSet, kLeft, kRight };

  // Returns text-align(only left and right are supported. see crbug.com/482339)
  // of text of element.
  Alignment AlignmentForFormData() const;

  // Returns direction of text of element.
  base::i18n::TextDirection DirectionForFormData() const;

  // Returns the name that should be used for the specified |element| when
  // storing autofill data.  This is either the field name or its id, an empty
  // string if it has no name and no id.
  WebString NameForAutofill() const;

  WebFormElement Form() const;
  // Returns the form that owns this element according to Autofill's definition
  // of ownership, or a null WebFormElement if no form owns it. The form that
  // owns this element is:
  // - if this element is associated to a form, the furthest shadow-including
  //   form ancestor of that form,
  // - otherwise, the furthest shadow-including form ancestor of this element.
  // For the definition of ownership in Autofill, see
  // //components/autofill/content/renderer/README.md.
  WebFormElement GetOwningFormForAutofill() const;

  // Returns the ax node id of the form control element in the accessibility
  // tree. The ax node id is consistent across renderer and browser processes.
  int32_t GetAxId() const;

  // If this element is a <textarea>, then returns a list with information (such
  // as typeface and glyphs) for the text inside. Otherwise returns
  // std::nullopt.
  std::optional<TextInfo> GetTextInfo() const;

#if INSIDE_BLINK
  WebFormControlElement(HTMLFormControlElement*);
  WebFormControlElement& operator=(HTMLFormControlElement*);
  operator HTMLFormControlElement*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebFormControlElement);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FORM_CONTROL_ELEMENT_H_
