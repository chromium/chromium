// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MODEL_EXECUTION_MOCK_MODEL_EXECUTION_SESSION_H_
#define CONTENT_BROWSER_MODEL_EXECUTION_MOCK_MODEL_EXECUTION_SESSION_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom.h"

// The mock implementation of `blink::mojom::ModelGenericSession` used for
// testing.
class MockModelExecutionSession : public blink::mojom::ModelGenericSession {
 public:
  MockModelExecutionSession();
  MockModelExecutionSession(const MockModelExecutionSession&) = delete;
  MockModelExecutionSession& operator=(const MockModelExecutionSession&) =
      delete;

  ~MockModelExecutionSession() override;

  // `blink::mojom::ModelGenericSession` implementation.
  void Execute(const std::string& input,
               mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                   pending_responder) override;
  void Destroy() override;

 private:
  void DoMockExecution(const std::string& input,
                       mojo::RemoteSetElementId responder_id);

  bool is_destroyed_ = false;
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  base::WeakPtrFactory<MockModelExecutionSession> weak_ptr_factory_{this};
};

#endif  // CONTENT_BROWSER_MODEL_EXECUTION_MOCK_MODEL_EXECUTION_SESSION_H_
