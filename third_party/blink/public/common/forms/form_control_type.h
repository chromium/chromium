// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FORMS_FORM_CONTROL_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FORMS_FORM_CONTROL_TYPE_H_

#include <cstdint>

namespace blink {

// An enum representation of the values of the `type` attribute of form control
// elements. This list is exhaustive.
enum class FormControlType : uint8_t {
  // https://html.spec.whatwg.org/multipage/form-elements.html#attr-button-type
  kButtonButton,
  kButtonSubmit,
  kButtonReset,
  kButtonSelectList,
  // https://html.spec.whatwg.org/multipage/form-elements.html#dom-fieldset-type
  kFieldset,
  // https://html.spec.whatwg.org/multipage/input.html#attr-input-type
  kInputButton,
  kInputCheckbox,
  kInputColor,
  kInputDate,
  kInputDatetimeLocal,
  kInputEmail,
  kInputFile,
  kInputHidden,
  kInputImage,
  kInputMonth,
  kInputNumber,
  kInputPassword,
  kInputRadio,
  kInputRange,
  kInputReset,
  kInputSearch,
  kInputSubmit,
  kInputTelephone,
  kInputText,
  kInputTime,
  kInputUrl,
  kInputWeek,
  // https://html.spec.whatwg.org/multipage/form-elements.html#dom-output-type
  kOutput,
  // https://html.spec.whatwg.org/multipage/form-elements.html#dom-select-type
  kSelectOne,
  kSelectMultiple,
  kSelectList,
  // https://html.spec.whatwg.org/multipage/form-elements.html#dom-textarea-type
  kTextArea,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FORMS_FORM_CONTROL_TYPE_H_
