// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_LOADER_H_

#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_model_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class DOMArrayBuffer;
class ExceptionState;
class ExecutionContext;
class MLContext;
class ScriptState;
class ScriptPromiseResolver;

class MODULES_EXPORT MLModelLoader final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MLModelLoader(ExecutionContext* execution_context,
                         MLContext* ml_context);

  static MLModelLoader* Create(ScriptState* script_state,
                               MLContext* ml_context,
                               ExceptionState& exception_state);

  MLModelLoader(const MLModelLoader&) = delete;
  MLModelLoader& operator=(const MLModelLoader&) = delete;

  ~MLModelLoader() override;

  // IDL Interface:
  ScriptPromise load(ScriptState* script_state,
                     DOMArrayBuffer* buffer,
                     ExceptionState& exception_state);

  void Trace(Visitor* visitor) const override;

  // The callback of loading model is used to bind the pending remote of `Model`
  // interface if the model is loaded successfully.
  using ModelLoadedCallback = base::OnceCallback<void(
      ml::model_loader::mojom::blink::LoadModelResult result,
      mojo::PendingRemote<ml::model_loader::mojom::blink::Model> pending_remote,
      ml::model_loader::mojom::blink::ModelInfoPtr model_info)>;
  void Load(ScriptState* script_state,
            DOMArrayBuffer* buffer,
            ModelLoadedCallback callback);

 private:
  void OnRemoteLoaderCreated(
      ScriptState* script_state,
      DOMArrayBuffer* buffer,
      ModelLoadedCallback callback,
      ml::model_loader::mojom::blink::CreateModelLoaderResult result,
      mojo::PendingRemote<ml::model_loader::mojom::blink::ModelLoader>
          pending_remote);

  Member<MLContext> ml_context_;

  HeapMojoRemote<ml::model_loader::mojom::blink::ModelLoader> remote_loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_LOADER_H_
