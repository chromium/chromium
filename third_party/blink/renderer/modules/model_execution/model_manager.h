// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/model_execution/model_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_model_generic_session_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class ModelGenericSession;
class V8GenericModelAvailability;

// The class that manages the exposed model APIs that load model assets and
// create ModelGenericSession.
class ModelManager final : public ScriptWrappable,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class ModelAvailability {
    kReadily = 0,
    kAfterDownload = 1,
    kNo = 2,

    kMaxValue = kNo,
  };

  explicit ModelManager(LocalDOMWindow* window);
  ~ModelManager() override = default;

  void Trace(Visitor* visitor) const override;

  // model_manager.idl implementation.
  ScriptPromiseTyped<V8GenericModelAvailability> canCreateGenericSession(
      ScriptState* script_state,
      ExceptionState& exception_state);
  ScriptPromiseTyped<ModelGenericSession> createGenericSession(
      ScriptState* script_state,
      ModelGenericSessionOptions* options,
      ExceptionState& exception_state);

 private:
  HeapMojoRemote<mojom::blink::ModelManager>& GetModelManagerRemote();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::ModelManager> model_manager_remote_{nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_MANAGER_H_
