// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_REWRITER_H_
#define CONTENT_BROWSER_AI_ECHO_AI_REWRITER_H_

#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIRewriter` which only echoes back
// the input text used for testing.
class EchoAIRewriter : public blink::mojom::AIRewriter {
 public:
  EchoAIRewriter() = default;
  EchoAIRewriter(const EchoAIRewriter&) = delete;
  EchoAIRewriter& operator=(const EchoAIRewriter&) = delete;

  ~EchoAIRewriter() override = default;

  // `blink::mojom::AIRewriter` implementation.
  void Rewrite(const std::string& input,
               const std::optional<std::string>& context,
               mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                   pending_responder) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_REWRITER_H_
