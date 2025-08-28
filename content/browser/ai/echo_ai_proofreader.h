// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_PROOFREADER_H_
#define CONTENT_BROWSER_AI_ECHO_AI_PROOFREADER_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom.h"

namespace content {

// The implementation of `blink::mojom::AIProofreader` which only echoes back
// the prompt text used for testing.
class EchoAIProofreader : public blink::mojom::AIProofreader {
 public:
  EchoAIProofreader();
  EchoAIProofreader(const EchoAIProofreader&) = delete;
  EchoAIProofreader& operator=(const EchoAIProofreader&) = delete;

  ~EchoAIProofreader() override;

  // `blink::mojom::AIProofreader` implementation.
  void Proofread(const std::string& input,
                 mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                     pending_responder) override;
  void GetCorrectionType(
      const std::string& input,
      const std::string& corrected_input,
      const std::string& correction,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) override;

 private:
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<EchoAIProofreader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_PROOFREADER_H_
