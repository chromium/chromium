/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_INPUT_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_INPUT_INFO_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_mode.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "ui/base/ime/text_input_action.h"

namespace blink {

struct WebTextInputInfo {
  WebTextInputType type;
  int flags;

  // The value of the currently focused input field.
  WebString value;

  // The cursor position of the current selection start, or the caret position
  // if nothing is selected.
  int selection_start;

  // The cursor position of the current selection end, or the caret position
  // if nothing is selected.
  int selection_end;

  // The start position of the current composition, or -1 if there is none.
  int composition_start;

  // The end position of the current composition, or -1 if there is none.
  int composition_end;

  // The inputmode attribute value of the currently focused input field.
  WebTextInputMode input_mode;

  // The enterkeyhint attribute value of the currently focused input field.
  ui::TextInputAction action;

  BLINK_PLATFORM_EXPORT bool Equals(const WebTextInputInfo&) const;

  WebTextInputInfo()
      : type(kWebTextInputTypeNone),
        flags(kWebTextInputFlagNone),
        selection_start(0),
        selection_end(0),
        composition_start(-1),
        composition_end(-1),
        input_mode(kWebTextInputModeDefault),
        action(ui::TextInputAction::kDefault) {}
};

inline bool operator==(const WebTextInputInfo& a, const WebTextInputInfo& b) {
  return a.Equals(b);
}

inline bool operator!=(const WebTextInputInfo& a, const WebTextInputInfo& b) {
  return !(a == b);
}

}  // namespace blink

#endif
