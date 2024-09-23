/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/color_chooser_ui_controller.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ColorChooserUIController::ColorChooserUIController(
    LocalFrame* frame,
    blink::ColorChooserClient* client)
    : chooser_(frame->DomWindow()->GetExecutionContext()),
      client_(client),
      frame_(frame),
      color_chooser_factory_(frame->DomWindow()->GetExecutionContext()),
      receiver_(this, frame->DomWindow()->GetExecutionContext()) {}

ColorChooserUIController::~ColorChooserUIController() = default;

void ColorChooserUIController::Trace(Visitor* visitor) const {
  visitor->Trace(color_chooser_factory_);
  visitor->Trace(receiver_);
  visitor->Trace(frame_);
  visitor->Trace(chooser_);
  visitor->Trace(client_);
  ColorChooser::Trace(visitor);
}

void ColorChooserUIController::OpenUI() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  OpenColorChooser();
#else
  NOTREACHED_IN_MIGRATION()
      << "ColorChooserUIController should only be used on Android or iOS";
#endif
}

void ColorChooserUIController::SetSelectedColor(const Color& color) {
  // Color can be set via JS before mojo OpenColorChooser completes.
  if (chooser_)
    chooser_->SetSelectedColor(color.Rgb());
}

void ColorChooserUIController::EndChooser() {
  chooser_.reset();
  client_->DidEndChooser();
}

AXObject* ColorChooserUIController::RootAXObject(Element* popup_owner) {
  return nullptr;
}

void ColorChooserUIController::DidChooseColor(uint32_t color) {
  client_->DidChooseColor(Color::FromRGBA32(color));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void ColorChooserUIController::OpenColorChooser() {
  DCHECK(!chooser_);
  scoped_refptr<base::SequencedTaskRunner> runner =
      frame_->DomWindow()->GetExecutionContext()->GetTaskRunner(
          TaskType::kUserInteraction);
  frame_->GetBrowserInterfaceBroker().GetInterface(
      color_chooser_factory_.BindNewPipeAndPassReceiver(runner));
  color_chooser_factory_->OpenColorChooser(
      chooser_.BindNewPipeAndPassReceiver(runner),
      receiver_.BindNewPipeAndPassRemote(runner), client_->CurrentColor().Rgb(),
      client_->Suggestions());
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &ColorChooserUIController::EndChooser, WrapWeakPersistent(this)));
}
#endif

}  // namespace blink
