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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_COLOR_CHOOSER_POPUP_UI_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_COLOR_CHOOSER_POPUP_UI_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_ui_controller.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"

namespace blink {

class ChromeClient;
class ColorChooserClient;
class PagePopup;

class CORE_EXPORT ColorChooserPopupUIController final
    : public ColorChooserUIController,
      public PagePopupClient {
  USING_PRE_FINALIZER(ColorChooserPopupUIController, Dispose);

 public:
  ColorChooserPopupUIController(LocalFrame*,
                                ChromeClient*,
                                blink::ColorChooserClient*);
  ~ColorChooserPopupUIController() override;
  void Trace(Visitor*) override;

  // ColorChooserUIController functions:
  void OpenUI() override;

  // ColorChooser functions
  void EndChooser() override;
  AXObject* RootAXObject() override;

  // PagePopupClient functions:
  void WriteDocument(SharedBuffer*) override;
  void SelectFontsFromOwnerDocument(Document&) override {}
  Locale& GetLocale() override;
  void SetValueAndClosePopup(int, const String&) override;
  void SetValue(const String&) override;
  void CancelPopup() override;
  Element& OwnerElement() override;
  void DidClosePopup() override;

 private:
  ChromeClient& GetChromeClient() override;

  void OpenPopup();
  void Dispose();

  void WriteColorPickerDocument(SharedBuffer*);
  void WriteColorSuggestionPickerDocument(SharedBuffer*);

  Member<ChromeClient> chrome_client_;
  PagePopup* popup_;
  Locale& locale_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_COLOR_CHOOSER_POPUP_UI_CONTROLLER_H_
