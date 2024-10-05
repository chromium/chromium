// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_ASSISTANT_H_
#define CONTENT_BROWSER_AI_ECHO_AI_ASSISTANT_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIAssistant` which only echoes
// back the prompt text used for testing.
class EchoAIAssistant : public blink::mojom::AIAssistant {
 public:
  EchoAIAssistant();
  EchoAIAssistant(const EchoAIAssistant&) = delete;
  EchoAIAssistant& operator=(const EchoAIAssistant&) = delete;

  ~EchoAIAssistant() override;

  // `blink::mojom::AIAssistant` implementation.
  void Prompt(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient>
                client) override;
  void Destroy() override;
  void CountPromptTokens(
      const std::string& input,
      mojo::PendingRemote<blink::mojom::AIAssistantCountPromptTokensClient>
          client) override;

 private:
  void DoMockExecution(const std::string& input,
                       mojo::RemoteSetElementId responder_id);

  bool is_destroyed_ = false;
  uint64_t current_tokens_ = 0;
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<EchoAIAssistant> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_ASSISTANT_H_
