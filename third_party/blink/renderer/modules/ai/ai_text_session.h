// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_

#include <variant>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class AIAssistant;
class AITextSessionFactory;

// TODO(crbug.com/365594095): replace this with `AIAssistantCreateClient` when
// implementing the abort signal.
// The class that represents a session with simple generic model execution. It's
// a simple wrapper of the `mojom::blink::AIAssistant` remote.
class AITextSession final : public GarbageCollected<AITextSession>,
                            public ExecutionContextClient {
 public:
  AITextSession(ExecutionContext* context,
                scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AITextSession() = default;

  void Trace(Visitor* visitor) const override;

  mojo::PendingReceiver<blink::mojom::blink::AIAssistant>
  GetModelSessionReceiver();

  HeapMojoRemote<blink::mojom::blink::AIAssistant>& GetAIRemote() {
    return assistant_remote_;
  }

  // These `SetInfo()` allows `AITextSessionFactory` (for session creation) and
  // `AIAssistant` (for session cloning) to set the info after getting it from
  // the remote.
  void SetInfo(std::variant<base::PassKey<AITextSessionFactory>,
                            base::PassKey<AIAssistant>> pass_key,
               blink::mojom::blink::AIAssistantInfoPtr info);

  const blink::mojom::blink::AIAssistantInfoPtr GetInfo() const {
    return info_.Clone();
  }

 private:
  blink::mojom::blink::AIAssistantInfoPtr info_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::AIAssistant> assistant_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_
