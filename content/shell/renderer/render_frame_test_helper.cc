// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/render_frame_test_helper.h"

#include <utility>

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

void RenderFrameTestHelper::Create(
    RenderFrame& render_frame,
    mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver) {
  new RenderFrameTestHelper(render_frame, std::move(receiver));
}

RenderFrameTestHelper::~RenderFrameTestHelper() {}

void RenderFrameTestHelper::GetDocumentToken(
    GetDocumentTokenCallback callback) {
  std::move(callback).Run(render_frame()->GetWebFrame()->GetDocument().Token());
}

void RenderFrameTestHelper::OnDestruct() {
  delete this;
}

RenderFrameTestHelper::RenderFrameTestHelper(
    RenderFrame& render_frame,
    mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver)
    : RenderFrameObserver(&render_frame),
      receiver_(this, std::move(receiver)) {}

}  // namespace content
