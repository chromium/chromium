// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_HOST_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/input/text_input_host.mojom.h"

namespace content {

// Note that the methods should run on BrowserThread::IO to get pumped because
// BrowserThread::UI is being blocked on a semaphore at TextInputClientMac.
// http://crbug.com/121917
class TextInputHostImpl : public blink::mojom::TextInputHost {
 public:
  TextInputHostImpl();

  TextInputHostImpl(const TextInputHostImpl&) = delete;
  TextInputHostImpl& operator=(const TextInputHostImpl&) = delete;

  ~TextInputHostImpl() override;

  static void Create(
      mojo::PendingReceiver<blink::mojom::TextInputHost> receiver);

  // blink::mojom::TextInputHost implementation.
  void GotCharacterIndexAtPoint(uint32_t index) override;
  void GotFirstRectForRange(const gfx::Rect& rect) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_HOST_IMPL_H_
