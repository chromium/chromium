// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_WRITER_H_
#define CONTENT_BROWSER_AI_ECHO_AI_WRITER_H_

#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIWriter` which only echoes back the
// input text used for testing.
class EchoAIWriter : public blink::mojom::AIWriter {
 public:
  EchoAIWriter() = default;
  EchoAIWriter(const EchoAIWriter&) = delete;
  EchoAIWriter& operator=(const EchoAIWriter&) = delete;

  ~EchoAIWriter() override = default;

  // `blink::mojom::AIWriter` implementation.
  void Write(const std::string& input,
             const std::optional<std::string>& context,
             mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                 pending_responder) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_WRITER_H_
