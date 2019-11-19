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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_COLOR_CHOOSER_UI_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_COLOR_CHOOSER_UI_CONTROLLER_H_

#include <memory>
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

class ColorChooserClient;
class LocalFrame;

class CORE_EXPORT ColorChooserUIController
    : public GarbageCollected<ColorChooserUIController>,
      public mojom::blink::ColorChooserClient,
      public ColorChooser {
  USING_GARBAGE_COLLECTED_MIXIN(ColorChooserUIController);
  USING_PRE_FINALIZER(ColorChooserUIController, Dispose);

 public:
  ColorChooserUIController(LocalFrame*, blink::ColorChooserClient*);
  ~ColorChooserUIController() override;
  void Trace(Visitor*) override;

  void Dispose();

  virtual void OpenUI();

  // ColorChooser functions:
  void SetSelectedColor(const Color&) final;
  void EndChooser() override;
  AXObject* RootAXObject() override;

  // mojom::blink::ColorChooserClient functions:
  void DidChooseColor(uint32_t color) final;

 protected:
  void OpenColorChooser();
  mojo::Remote<mojom::blink::ColorChooser> chooser_;
  Member<blink::ColorChooserClient> client_;

  Member<LocalFrame> frame_;

 private:
  mojo::Remote<mojom::blink::ColorChooserFactory> color_chooser_factory_;
  mojo::Receiver<mojom::blink::ColorChooserClient> receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_COLOR_CHOOSER_UI_CONTROLLER_H_
