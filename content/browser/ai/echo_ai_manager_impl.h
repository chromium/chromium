// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_
#define CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_

#include <variant>

#include "base/no_destructor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIManager` that creates session which
// only echoes back the prompt text used for testing, and all the parameters
// will be set using the default value.
class EchoAIManagerImpl : public blink::mojom::AIManager {
 public:
  using ReceiverContext =
      std::variant<RenderFrameHost*, base::SupportsUserData*>;

  EchoAIManagerImpl(const EchoAIManagerImpl&) = delete;
  EchoAIManagerImpl& operator=(const EchoAIManagerImpl&) = delete;

  ~EchoAIManagerImpl() override;

  static void Create(content::BrowserContext* browser_context,
                     ReceiverContext context,
                     mojo::PendingReceiver<blink::mojom::AIManager> receiver);

 private:
  friend base::NoDestructor<EchoAIManagerImpl>;

  EchoAIManagerImpl(content::BrowserContext* browser_context,
                    ReceiverContext context);

  // `blink::mojom::AIManager` implementation.
  void CanCreateAssistant(CanCreateAssistantCallback callback) override;

  void CreateAssistant(
      mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient> client,
      blink::mojom::AIAssistantCreateOptionsPtr options) override;

  void CanCreateSummarizer(CanCreateSummarizerCallback callback) override;

  void CreateSummarizer(
      mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
      blink::mojom::AISummarizerCreateOptionsPtr options) override;

  void GetModelInfo(GetModelInfoCallback callback) override;
  void CreateWriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
      blink::mojom::AIWriterCreateOptionsPtr options) override;
  void CreateRewriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
      blink::mojom::AIRewriterCreateOptionsPtr options) override;

  mojo::ReceiverSet<blink::mojom::AIManager, ReceiverContext> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_MANAGER_IMPL_H_
