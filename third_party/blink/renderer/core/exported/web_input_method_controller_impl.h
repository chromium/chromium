// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_INPUT_METHOD_CONTROLLER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_INPUT_METHOD_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class InputMethodController;
class LocalFrame;
class WebLocalFrameImpl;
class WebPlugin;
class WebRange;
class WebString;

class CORE_EXPORT WebInputMethodControllerImpl
    : public WebInputMethodController {
  DISALLOW_NEW();

 public:
  explicit WebInputMethodControllerImpl(WebLocalFrameImpl& web_frame);
  ~WebInputMethodControllerImpl() override;

  // WebInputMethodController overrides.
  bool SetComposition(const WebString& text,
                      const WebVector<WebImeTextSpan>& ime_text_spans,
                      const WebRange& replacement_range,
                      int selection_start,
                      int selection_end) override;
  bool CommitText(const WebString& text,
                  const WebVector<WebImeTextSpan>& ime_text_spans,
                  const WebRange& replacement_range,
                  int relative_caret_position) override;
  bool FinishComposingText(
      ConfirmCompositionBehavior selection_behavior) override;
  WebTextInputInfo TextInputInfo() override;
  int ComputeWebTextInputNextPreviousFlags() override;
  WebTextInputType TextInputType() override;
  WebRange CompositionRange() override;
  bool GetCompositionCharacterBounds(WebVector<WebRect>& bounds) override;

  WebRange GetSelectionOffsets() const override;

  void GetLayoutBounds(WebRect& control_bounds,
                       WebRect& selection_bounds) override;
  bool IsEditContextActive() const override;

  void Trace(blink::Visitor*);

 private:
  LocalFrame* GetFrame() const;
  InputMethodController& GetInputMethodController() const;
  WebPlugin* FocusedPluginIfInputMethodSupported() const;

  const Member<WebLocalFrameImpl> web_frame_;

  DISALLOW_COPY_AND_ASSIGN(WebInputMethodControllerImpl);
};
}  // namespace blink

#endif
