// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_TEXT_INPUT_CONTROLLER_H_
#define PPAPI_CPP_TEXT_INPUT_CONTROLLER_H_

#include <stdint.h>

#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the APIs for text input handling.

namespace pp {

class Rect;

/// This class can be used for giving hints to the browser about the text input
/// status of plugins.
class TextInputController {
 public:
  /// A constructor for creating a <code>TextInputController</code>.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit TextInputController(const InstanceHandle& instance);

  /// Destructor.
  ~TextInputController();

  /// SetTextInputType() informs the browser about the current text input mode
  /// of the plugin.
  ///
  /// @param[in] type The type of text input type.
  void SetTextInputType(PP_TextInput_Type type);

  /// UpdateCaretPosition() informs the browser about the coordinates of the
  /// text input caret area.
  ///
  /// @param[in] caret A rectangle indicating the caret area.
  void UpdateCaretPosition(const Rect& caret);

  /// CancelCompositionText() informs the browser that the current composition
  /// text is cancelled by the plugin.
  void CancelCompositionText();

  /// UpdateSurroundingText() informs the browser about the current text
  /// selection and surrounding text.
  ///
  /// @param[in] text A UTF-8 sting indicating string buffer of current input
  /// context.
  ///
  /// @param[in] caret A integer indicating the byte index of caret location in
  /// <code>text</code>.
  ///
  /// @param[in] caret A integer indicating the byte index of anchor location in
  /// <code>text</code>. If there is no selection, this value should be equal to
  /// <code>caret</code>.
  void UpdateSurroundingText(const Var& text,
                             uint32_t caret,
                             uint32_t anchor);

 private:
  InstanceHandle instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_TEXT_INPUT_CONTROLLER_H_
