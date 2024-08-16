// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_FACTORY_H_

#include <optional>

#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_capability_availability.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class AITextSession;

// This class is responsible for creating AITextSession instances.
class AITextSessionFactory : public GarbageCollected<AITextSessionFactory>,
                             public ExecutionContextClient {
 public:
  using CanCreateTextSessionCallback =
      base::OnceCallback<void(AICapabilityAvailability,
                              mojom::blink::ModelAvailabilityCheckResult)>;
  using CreateTextSessionCallback =
      base::OnceCallback<void(base::expected<AITextSession*, DOMException*>)>;

  AITextSessionFactory(ExecutionContext* context,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual ~AITextSessionFactory() = default;

  void Trace(Visitor* visitor) const override;

  void CanCreateTextSession(CanCreateTextSessionCallback callback);
  // The sampling_params can be nullptr and the default value will be used.
  void CreateTextSession(
      mojom::blink::AITextSessionSamplingParamsPtr sampling_params,
      const WTF::String& system_prompt,
      CreateTextSessionCallback callback);

 private:
  HeapMojoRemote<mojom::blink::AIManager>& GetAIRemote();

  HeapMojoRemote<mojom::blink::AIManager> ai_remote_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_FACTORY_H_
