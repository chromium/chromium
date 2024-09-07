// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_text_session.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

AITextSession::AITextSession(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      text_session_remote_(context) {}

void AITextSession::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(text_session_remote_);
}

mojo::PendingReceiver<blink::mojom::blink::AITextSession>
AITextSession::GetModelSessionReceiver() {
  return text_session_remote_.BindNewPipeAndPassReceiver(task_runner_);
}

void AITextSession::SetInfo(std::variant<base::PassKey<AITextSessionFactory>,
                                         base::PassKey<AIAssistant>> pass_key,
                            blink::mojom::blink::AITextSessionInfoPtr info) {
  CHECK(!info_) << "The session info should only be set once after creation";
  info_ = std::move(info);
}

}  // namespace blink
