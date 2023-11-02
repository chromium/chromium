// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/text_input_controller.h"

#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TextInputController_1_0>() {
  return PPB_TEXTINPUTCONTROLLER_INTERFACE_1_0;
}

}  // namespace


TextInputController::TextInputController(const InstanceHandle& instance)
    : instance_(instance) {
}

TextInputController::~TextInputController() {
}

void TextInputController::SetTextInputType(PP_TextInput_Type type) {
  if (has_interface<PPB_TextInputController_1_0>()) {
    get_interface<PPB_TextInputController_1_0>()->SetTextInputType(
        instance_.pp_instance(), type);
  }
}

void TextInputController::UpdateCaretPosition(const Rect& caret) {
  if (has_interface<PPB_TextInputController_1_0>()) {
    get_interface<PPB_TextInputController_1_0>()->UpdateCaretPosition(
        instance_.pp_instance(), &caret.pp_rect());
  }
}

void TextInputController::CancelCompositionText() {
  if (has_interface<PPB_TextInputController_1_0>()) {
    get_interface<PPB_TextInputController_1_0>()->CancelCompositionText(
        instance_.pp_instance());
  }
}

void TextInputController::UpdateSurroundingText(const Var& text,
                                                uint32_t caret,
                                                uint32_t anchor) {
  if (has_interface<PPB_TextInputController_1_0>()) {
    get_interface<PPB_TextInputController_1_0>()->UpdateSurroundingText(
        instance_.pp_instance(),
        text.pp_var(),
        caret,
        anchor);
  }
}


}  // namespace pp
