// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_SEMANTIC_EMBEDDER_H_
#define CONTENT_BROWSER_AI_ECHO_AI_SEMANTIC_EMBEDDER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "third_party/blink/public/mojom/ai/ai_semantic_embedder.mojom.h"

namespace content {

class EchoAISemanticEmbedder : public blink::mojom::AISemanticEmbedder {
 public:
  EchoAISemanticEmbedder();
  EchoAISemanticEmbedder(const EchoAISemanticEmbedder&) = delete;
  EchoAISemanticEmbedder& operator=(const EchoAISemanticEmbedder&) = delete;
  ~EchoAISemanticEmbedder() override;

  // blink::mojom::AISemanticEmbedder:
  void Embed(uint64_t request_id,
             const std::vector<std::string>& inputs,
             blink::mojom::AISemanticEmbedderEmbedOptionsPtr options,
             EmbedCallback callback) override;
  void CancelEmbed(uint64_t request_id) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_SEMANTIC_EMBEDDER_H_
