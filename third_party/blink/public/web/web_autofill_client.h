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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AUTOFILL_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AUTOFILL_CLIENT_H_

namespace blink {

class WebFormControlElement;
class WebInputElement;
class WebKeyboardEvent;
class WebNode;

class WebAutofillClient {
 public:
  // These methods are called when the users edits a text-field.
  virtual void TextFieldDidEndEditing(const WebInputElement&) {}
  virtual void TextFieldDidChange(const WebFormControlElement&) {}
  virtual void TextFieldDidReceiveKeyDown(const WebInputElement&,
                                          const WebKeyboardEvent&) {}
  // This is called when a datalist indicator is clicked.
  virtual void OpenTextDataListChooser(const WebInputElement&) {}
  // This is called when the datalist for an input has changed.
  virtual void DataListOptionsChanged(const WebInputElement&) {}

  // Called when selected option of select control change.
  virtual void SelectControlDidChange(const WebFormControlElement&) {}

  // Called when the options of a select control change.
  virtual void SelectFieldOptionsChanged(const WebFormControlElement&) {}

  // Called when the user interacts with the page after a load.
  virtual void UserGestureObserved() {}

  virtual void DidAssociateFormControlsDynamically() {}
  virtual void AjaxSucceeded() {}

  virtual void DidCompleteFocusChangeInFrame() {}
  virtual void DidReceiveLeftMouseDownOrGestureTapInNode(const WebNode&) {}

  // Asks the client whether to suppess the keyboard for the given control
  // element.
  virtual bool ShouldSuppressKeyboard(const WebFormControlElement&) {
    return false;
  }

 protected:
  virtual ~WebAutofillClient() = default;
};

}  // namespace blink

#endif
