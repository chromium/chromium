// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_SUMMARIZER_H_
#define CONTENT_BROWSER_AI_ECHO_AI_SUMMARIZER_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom.h"

namespace content {

// The implementation of `blink::mojom::AISummarizer` which only echoes back the
// prompt text used for testing.
class EchoAISummarizer : public blink::mojom::AISummarizer {
 public:
  EchoAISummarizer();
  EchoAISummarizer(const EchoAISummarizer&) = delete;
  EchoAISummarizer& operator=(const EchoAISummarizer&) = delete;

  ~EchoAISummarizer() override;

  // `blink::mojom::AISummarizer` implementation.
  void Summarize(const std::string& input,
                 const std::string& context,
                 mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                     pending_responder) override;

 private:
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<EchoAISummarizer> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_SUMMARIZER_H_
