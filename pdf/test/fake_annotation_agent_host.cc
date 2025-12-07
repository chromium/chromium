// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/fake_annotation_agent_host.h"

namespace chrome_pdf {

FakeAnnotationAgentHost::FakeAnnotationAgentHost(
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
        annotation_agent_host_receiver,
    mojo::PendingRemote<blink::mojom::AnnotationAgent> annotation_agent_remote)
    : annotation_agent_host_receiver_(
          this,
          std::move(annotation_agent_host_receiver)),
      annotation_agent_remote_(std::move(annotation_agent_remote)) {}

FakeAnnotationAgentHost::~FakeAnnotationAgentHost() = default;

void FakeAnnotationAgentHost::DidFinishAttachment(
    const gfx::Rect& document_relative_rect,
    blink::mojom::AttachmentResult attachment_result) {
  attachment_result_ = attachment_result;
  run_loop_.Quit();
}

void FakeAnnotationAgentHost::ScrollIntoView() {
  annotation_agent_remote_->ScrollIntoView(/*applies_focus=*/true);
  annotation_agent_remote_.FlushForTesting();
}

blink::mojom::AttachmentResult
FakeAnnotationAgentHost::WaitForAttachmentResult() {
  run_loop_.Run();
  return *attachment_result_;
}

}  // namespace chrome_pdf
