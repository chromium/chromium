// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/text_input_dev.h"

#include "ppapi/c/dev/ppp_text_input_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/rect.h"

namespace pp {

namespace {

static const char kPPPTextInputInterface[] = PPP_TEXTINPUT_DEV_INTERFACE;

void RequestSurroundingText(PP_Instance instance,
                            uint32_t desired_number_of_characters) {
  void* object = Instance::GetPerInstanceObject(instance,
                                                kPPPTextInputInterface);
  if (!object)
    return;
  static_cast<TextInput_Dev*>(object)->RequestSurroundingText(
      desired_number_of_characters);
}

const PPP_TextInput_Dev ppp_text_input = {
  &RequestSurroundingText
};

template <> const char* interface_name<PPB_TextInput_Dev_0_2>() {
  return PPB_TEXTINPUT_DEV_INTERFACE_0_2;
}

template <> const char* interface_name<PPB_TextInput_Dev_0_1>() {
  return PPB_TEXTINPUT_DEV_INTERFACE_0_1;
}

}  // namespace


TextInput_Dev::TextInput_Dev(Instance* instance)
    : instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPTextInputInterface,
                                    &ppp_text_input);
  instance->AddPerInstanceObject(kPPPTextInputInterface, this);
}

TextInput_Dev::~TextInput_Dev() {
  Instance::RemovePerInstanceObject(instance_, kPPPTextInputInterface, this);
}

void TextInput_Dev::RequestSurroundingText(uint32_t) {
  // Default implementation. Send a null range.
  UpdateSurroundingText(std::string(), 0, 0);
}

void TextInput_Dev::SetTextInputType(PP_TextInput_Type_Dev type) {
  if (has_interface<PPB_TextInput_Dev_0_2>()) {
    get_interface<PPB_TextInput_Dev_0_2>()->SetTextInputType(
        instance_.pp_instance(), type);
  } else if (has_interface<PPB_TextInput_Dev_0_1>()) {
    get_interface<PPB_TextInput_Dev_0_1>()->SetTextInputType(
        instance_.pp_instance(), type);
  }
}

void TextInput_Dev::UpdateCaretPosition(const Rect& caret,
                                        const Rect& bounding_box) {
  if (has_interface<PPB_TextInput_Dev_0_2>()) {
    get_interface<PPB_TextInput_Dev_0_2>()->UpdateCaretPosition(
        instance_.pp_instance(), &caret.pp_rect(), &bounding_box.pp_rect());
  } else if (has_interface<PPB_TextInput_Dev_0_1>()) {
    get_interface<PPB_TextInput_Dev_0_1>()->UpdateCaretPosition(
        instance_.pp_instance(), &caret.pp_rect(), &bounding_box.pp_rect());
  }
}

void TextInput_Dev::CancelCompositionText() {
  if (has_interface<PPB_TextInput_Dev_0_2>()) {
    get_interface<PPB_TextInput_Dev_0_2>()->CancelCompositionText(
        instance_.pp_instance());
  } else if (has_interface<PPB_TextInput_Dev_0_1>()) {
    get_interface<PPB_TextInput_Dev_0_1>()->CancelCompositionText(
        instance_.pp_instance());
  }
}

void TextInput_Dev::SelectionChanged() {
  if (has_interface<PPB_TextInput_Dev_0_2>()) {
    get_interface<PPB_TextInput_Dev_0_2>()->SelectionChanged(
        instance_.pp_instance());
  }
}

void TextInput_Dev::UpdateSurroundingText(const std::string& text,
                                          uint32_t caret,
                                          uint32_t anchor) {
  if (has_interface<PPB_TextInput_Dev_0_2>()) {
    get_interface<PPB_TextInput_Dev_0_2>()->UpdateSurroundingText(
        instance_.pp_instance(), text.c_str(), caret, anchor);
  }
}


}  // namespace pp
