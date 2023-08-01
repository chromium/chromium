// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_host_impl.h"

#include "content/browser/renderer_host/text_input_client_mac.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

TextInputHostImpl::TextInputHostImpl() = default;
TextInputHostImpl::~TextInputHostImpl() = default;

void TextInputHostImpl::Create(
    mojo::PendingReceiver<blink::mojom::TextInputHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeSelfOwnedReceiver(std::make_unique<TextInputHostImpl>(),
                              std::move(receiver));
}

void TextInputHostImpl::GotCharacterIndexAtPoint(uint32_t index) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TextInputClientMac* service = TextInputClientMac::GetInstance();
  service->SetCharacterIndexAndSignal(index);
}

void TextInputHostImpl::GotFirstRectForRange(const gfx::Rect& rect) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TextInputClientMac* service = TextInputClientMac::GetInstance();
  service->SetFirstRectAndSignal(rect);
}

}  // namespace content
