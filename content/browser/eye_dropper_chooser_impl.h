// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_EYE_DROPPER_CHOOSER_IMPL_H_
#define CONTENT_BROWSER_EYE_DROPPER_CHOOSER_IMPL_H_

#include <memory>

#include "content/public/browser/document_service.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

namespace content {

class EyeDropper;
class EyeDropperListener;

class EyeDropperChooserImpl final
    : public DocumentService<blink::mojom::EyeDropperChooser>,
      public EyeDropperListener {
 public:
  static void Create(RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::EyeDropperChooser>);

  EyeDropperChooserImpl(const EyeDropperChooserImpl&) = delete;
  EyeDropperChooserImpl& operator=(const EyeDropperChooserImpl&) = delete;

  // EyeDropperChooser:
  void Choose(ChooseCallback) override;

  // EyeDropperListener:
  void ColorSelected(SkColor color) override;
  void ColorSelectionCanceled() override;

 private:
  EyeDropperChooserImpl(RenderFrameHost&,
                        mojo::PendingReceiver<blink::mojom::EyeDropperChooser>);

  ~EyeDropperChooserImpl() override;

  ChooseCallback callback_;
  std::unique_ptr<EyeDropper> eye_dropper_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_EYE_DROPPER_CHOOSER_IMPL_H_
