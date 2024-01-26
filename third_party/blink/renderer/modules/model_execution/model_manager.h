// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/model_execution/model_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// The class that manages the exposed model APIs that load model assets and
// create ModelGenericSession.
class ModelManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Availability { kReadily, kAfterDownload, kNo };

  explicit ModelManager(LocalDOMWindow* window);
  ~ModelManager() override = default;

  void Trace(Visitor* visitor) const override;

  // model_manager.idl implementation.
  ScriptPromise canCreateGenericSession(ScriptState* script_state,
                                        ExceptionState& exception_state);
  ScriptPromise createGenericSession(ScriptState* script_state,
                                     ExceptionState& exception_state);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::ModelManager> model_manager_remote_{nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_MANAGER_H_
