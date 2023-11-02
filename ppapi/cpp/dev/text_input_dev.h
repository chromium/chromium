// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_TEXT_INPUT_DEV_H_
#define PPAPI_CPP_DEV_TEXT_INPUT_DEV_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class Rect;
class Instance;

// This class allows you to associate the PPP_TextInput_Dev and
// PPB_TextInput_Dev C-based interfaces with an object. It associates itself
// with the given instance, and registers as the global handler for handling the
// PPP_TextInput_Dev interface that the browser calls.
//
// You would typically use this either via inheritance on your instance:
//   class MyInstance : public pp::Instance, public pp::TextInput_Dev {
//     MyInstance() : pp::TextInput_Dev(this) {
//     }
//     ...
//   };
//
// or by composition:
//   class MyTextInput : public pp::TextInput_Dev {
//     ...
//   };
//
//   class MyInstance : public pp::Instance {
//     MyInstance() : text_input_(this) {
//     }
//
//     MyTextInput text_input_;
//   };
class TextInput_Dev {
 public:
  explicit TextInput_Dev(Instance* instance);
  virtual ~TextInput_Dev();

  virtual void RequestSurroundingText(uint32_t desired_number_of_characters);

  void SetTextInputType(PP_TextInput_Type_Dev type);
  void UpdateCaretPosition(const Rect& caret, const Rect& bounding_box);
  void CancelCompositionText();
  void SelectionChanged();
  void UpdateSurroundingText(const std::string& text,
                             uint32_t caret, uint32_t anchor);

 private:
  InstanceHandle instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_TEXT_INPUT_DEV_H_
