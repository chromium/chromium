// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_TEXT_INPUT_CONTROLLER_H_
#define CONTENT_WEB_TEST_RENDERER_TEXT_INPUT_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace blink {
class WebInputMethodController;
class WebLocalFrame;
class WebView;
}  // namespace blink

namespace content {

class WebFrameTestProxy;

// TextInputController is bound to window.textInputController in Javascript
// when content_shell is running. Web tests use it to exercise various
// corners of text input.
class TextInputController {
 public:
  explicit TextInputController(WebFrameTestProxy* web_frame_test_proxy);

  TextInputController(const TextInputController&) = delete;
  TextInputController& operator=(const TextInputController&) = delete;

  ~TextInputController();

  void Install(blink::WebLocalFrame* frame);

 private:
  friend class TextInputControllerBindings;

  void InsertText(const std::string& text);
  void UnmarkText();
  void UnmarkAndUnselectText();
  void DoCommand(const std::string& text);
  void ExtendSelectionAndDelete(int before, int after);
  void DeleteSurroundingText(int before, int after);
  void SetMarkedText(const std::string& text, uint32_t start, uint32_t length);
  void SetMarkedTextFromExistingText(uint32_t start, uint32_t end);
  bool HasMarkedText();
  std::vector<int> MarkedRange();
  std::vector<int> SelectedRange();
  std::vector<int> FirstRectForCharacterRange(uint32_t location,
                                              uint32_t length);
  void SetComposition(const std::string& text,
                      int replacement_range_start,
                      int replacement_range_end);
  void ForceTextInputStateUpdate();

  blink::WebView* view();
  // Returns the WebInputMethodController corresponding to the focused frame
  // accepting IME. Could return nullptr if no such frame exists.
  blink::WebInputMethodController* GetInputMethodController();

  const raw_ptr<WebFrameTestProxy> web_frame_test_proxy_;

  base::WeakPtrFactory<TextInputController> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TEXT_INPUT_CONTROLLER_H_
