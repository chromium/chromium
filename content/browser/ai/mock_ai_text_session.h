// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_MOCK_AI_TEXT_SESSION_H_
#define CONTENT_BROWSER_AI_MOCK_AI_TEXT_SESSION_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"

namespace content {

// The mock implementation of `blink::mojom::ModelGenericSession` used for
// testing.
class MockAITextSession : public blink::mojom::AITextSession {
 public:
  MockAITextSession();
  MockAITextSession(const MockAITextSession&) = delete;
  MockAITextSession& operator=(const MockAITextSession&) = delete;

  ~MockAITextSession() override;

  // `blink::mojom::ModelGenericSession` implementation.
  void Prompt(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Destroy() override;

 private:
  void DoMockExecution(const std::string& input,
                       mojo::RemoteSetElementId responder_id);

  bool is_destroyed_ = false;
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<MockAITextSession> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_MOCK_AI_TEXT_SESSION_H_
