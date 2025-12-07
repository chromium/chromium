// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_FAKE_ANNOTATION_AGENT_HOST_H_
#define PDF_TEST_FAKE_ANNOTATION_AGENT_HOST_H_

#include <optional>

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

// A test-only browser-side IPC endpoint.
class FakeAnnotationAgentHost : public blink::mojom::AnnotationAgentHost {
 public:
  FakeAnnotationAgentHost(
      mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
          annotation_agent_host_receiver,
      mojo::PendingRemote<blink::mojom::AnnotationAgent>
          annotation_agent_remote);
  ~FakeAnnotationAgentHost() override;

  // `blink::mojom::AnnotationAgentHost`:
  void DidFinishAttachment(
      const gfx::Rect& document_relative_rect,
      blink::mojom::AttachmentResult attachment_result) override;

  void ScrollIntoView();

  blink::mojom::AttachmentResult WaitForAttachmentResult();

 private:
  mojo::Receiver<blink::mojom::AnnotationAgentHost>
      annotation_agent_host_receiver_;
  mojo::Remote<blink::mojom::AnnotationAgent> annotation_agent_remote_;

  std::optional<blink::mojom::AttachmentResult> attachment_result_;
  base::RunLoop run_loop_;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_FAKE_ANNOTATION_AGENT_HOST_H_
