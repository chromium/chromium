// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_HOST_IMPL_H_

#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/input/text_input_host.mojom.h"

namespace base {
class UnguessableToken;
}

namespace content {

// A class to implement the incoming response messages from the renderer process
// for IME IPCs. See the class comment on TextInputClientMac.
class CONTENT_EXPORT TextInputHostImpl : public blink::mojom::TextInputHost {
 public:
  TextInputHostImpl();

  TextInputHostImpl(const TextInputHostImpl&) = delete;
  TextInputHostImpl& operator=(const TextInputHostImpl&) = delete;

  ~TextInputHostImpl() override;

  static void Create(
      mojo::PendingReceiver<blink::mojom::TextInputHost> receiver);

  // blink::mojom::TextInputHost implementation.
  void GotCharacterIndexAtPoint(const base::UnguessableToken& request_token,
                                uint32_t index) override;
  void GotFirstRectForRange(const base::UnguessableToken& request_token,
                            const gfx::Rect& rect) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_HOST_IMPL_H_
