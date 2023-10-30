// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_model_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ML;
class MLContextOptions;
class MLModelLoader;

class MODULES_EXPORT MLContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MLContext* ValidateAndCreateSync(MLContextOptions* options, ML* ml);

  // The constructor shouldn't be called directly. The callers should use
  // CreateAsync() or CreateSync() method instead.
  MLContext(const V8MLDevicePreference device_preference,
            const V8MLDeviceType device_type,
            const V8MLPowerPreference power_preference,
            const V8MLModelFormat model_format,
            const unsigned int num_threads,
            ML* ml);

  MLContext(const MLContext&) = delete;
  MLContext& operator=(const MLContext&) = delete;

  ~MLContext() override;

  V8MLDevicePreference GetDevicePreference() const;
  V8MLDeviceType GetDeviceType() const;
  V8MLPowerPreference GetPowerPreference() const;
  V8MLModelFormat GetModelFormat() const;
  unsigned int GetNumThreads() const;
  void LogConsoleWarning(const String& message);

  ML* GetML();
  // This method returns a MLModelLoader that's used and shared by WebNN APIs
  // invoked on this MLContext.
  MLModelLoader* GetModelLoaderForWebNN(ScriptState* script_state);

  void Trace(Visitor* visitor) const override;

  // IDL interface:
  ScriptPromise compute(ScriptState* script_state,
                        MLGraph* graph,
                        const MLNamedArrayBufferViews& inputs,
                        const MLNamedArrayBufferViews& outputs,
                        ExceptionState& exception_state);

  void computeSync(MLGraph* graph,
                   const MLNamedArrayBufferViews& inputs,
                   const MLNamedArrayBufferViews& outputs,
                   ExceptionState& exception_state);

 protected:
  // Create and initialize a MLContext object. Resolve the promise with
  // this concrete object if the underlying context gets created
  // successfully.
  void CreateAsync(ScriptPromiseResolver* resolver, MLContextOptions* options);

  // An MLContext backend should implement this method to create and initialize
  // a platform specific context asynchronously.
  virtual void CreateAsyncImpl(ScriptPromiseResolver* resolver,
                               MLContextOptions* options);

  // CreateSync() has the similar function as CreateAsync(). The difference is
  // if there are no validation error, it calls CreateSyncImpl() implemented
  // by a MLContext backend that initializes the context synchronously in the
  // caller's thread. This method is called by ML to implement
  // MLContext.createContextSync() method.
  MLContext* CreateSync(ScriptState* script_state,
                        MLContextOptions* options,
                        ExceptionState& exception_state);

  // An MLContext backend should implement this method to initialize the
  // platform context synchronously in the caller's thread.
  virtual MLContext* CreateSyncImpl(ScriptState* script_state,
                                    MLContextOptions* options,
                                    ExceptionState& exception_state);

 private:
  V8MLDevicePreference device_preference_;
  V8MLDeviceType device_type_;
  V8MLPowerPreference power_preference_;
  V8MLModelFormat model_format_;
  unsigned int num_threads_;

  Member<ML> ml_;
  // WebNN uses this MLModelLoader to build a computational graph.
  Member<MLModelLoader> ml_model_loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_CONTEXT_H_
