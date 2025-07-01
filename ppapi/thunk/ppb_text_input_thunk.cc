// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enum: " #a)

STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_NONE, PP_TEXTINPUT_TYPE_NONE);
STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_TEXT, PP_TEXTINPUT_TYPE_TEXT);
STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_PASSWORD, PP_TEXTINPUT_TYPE_PASSWORD);
STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_SEARCH, PP_TEXTINPUT_TYPE_SEARCH);
STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_EMAIL, PP_TEXTINPUT_TYPE_EMAIL);
STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_NUMBER, PP_TEXTINPUT_TYPE_NUMBER);
STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_TELEPHONE,
                   PP_TEXTINPUT_TYPE_TELEPHONE);
STATIC_ASSERT_ENUM(PP_TEXTINPUT_TYPE_DEV_URL, PP_TEXTINPUT_TYPE_URL);

void SetTextInputType(PP_Instance instance, PP_TextInput_Type type) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->SetTextInputType(instance, type);
}

void SetTextInputType_0_2(PP_Instance instance, PP_TextInput_Type_Dev type) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->SetTextInputType(instance,
                                        static_cast<PP_TextInput_Type>(type));
}

void UpdateCaretPosition_0_2(PP_Instance instance,
                         const PP_Rect* caret,
                         const PP_Rect* bounding_box) {
  EnterInstance enter(instance);
  if (enter.succeeded() && caret && bounding_box)
    enter.functions()->UpdateCaretPosition(instance, *caret, *bounding_box);
}

void UpdateCaretPosition(PP_Instance instance,
                         const PP_Rect* caret) {
  EnterInstance enter(instance);
  if (enter.succeeded() && caret)
    enter.functions()->UpdateCaretPosition(instance, *caret, PP_Rect());
}

void CancelCompositionText(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->CancelCompositionText(instance);
}

void UpdateSurroundingText_0_2(PP_Instance instance, const char* text,
                               uint32_t caret, uint32_t anchor) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->UpdateSurroundingText(instance, text, caret, anchor);
}

void UpdateSurroundingText_1_0(PP_Instance instance, PP_Var text,
                               uint32_t caret, uint32_t anchor) {
  EnterInstance enter(instance);
  StringVar* var = StringVar::FromPPVar(text);
  if (enter.succeeded() && var)
    enter.functions()->UpdateSurroundingText(instance,
                                             var->value().c_str(),
                                             caret,
                                             anchor);
}

void SelectionChanged(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->SelectionChanged(instance);
}

const PPB_TextInput_Dev_0_1 g_ppb_textinput_0_1_thunk = {
  &SetTextInputType_0_2,
  &UpdateCaretPosition_0_2,
  &CancelCompositionText,
};

const PPB_TextInput_Dev_0_2 g_ppb_textinput_0_2_thunk = {
  &SetTextInputType_0_2,
  &UpdateCaretPosition_0_2,
  &CancelCompositionText,
  &UpdateSurroundingText_0_2,
  &SelectionChanged,
};

const PPB_TextInputController_1_0 g_ppb_textinputcontroller_1_0_thunk = {
  &SetTextInputType,
  &UpdateCaretPosition,
  &CancelCompositionText,
  &UpdateSurroundingText_1_0,
};

}  // namespace

const PPB_TextInput_Dev_0_1* GetPPB_TextInput_Dev_0_1_Thunk() {
  return &g_ppb_textinput_0_1_thunk;
}

const PPB_TextInput_Dev_0_2* GetPPB_TextInput_Dev_0_2_Thunk() {
  return &g_ppb_textinput_0_2_thunk;
}

const PPB_TextInputController_1_0* GetPPB_TextInputController_1_0_Thunk() {
  return &g_ppb_textinputcontroller_1_0_thunk;
}

}  // namespace thunk
}  // namespace ppapi
