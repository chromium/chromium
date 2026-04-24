// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/advanced_memory_safety_checks.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace ml {

class ChromeMLHolder;

class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) ChromeML {
  // TODO(crbug.com/487051617): Remove this macro once the bug gets fixed.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  ~ChromeML();
  ChromeML(const ChromeML& other) = delete;
  ChromeML& operator=(const ChromeML& other) = delete;
  ChromeML(ChromeML&& other) = delete;
  ChromeML& operator=(ChromeML&& other) = delete;

  // Gets a lazily initialized global instance of ChromeML. May return null
  // if the underlying library could not be loaded.
  static ChromeML* Get();

  // Creates a new instance of ChromeML. May return null if the underlying
  // library could not be loaded.
  static std::unique_ptr<ChromeML> CreateForTesting(
      const std::optional<std::string>& library_name = std::nullopt);

  // Creates a new instance of ChromeML using the provided ChromeMLAPI.
  static std::unique_ptr<ChromeML> CreateForTesting(const ChromeMLAPI* api);

  // Wrappers for C-API function pointers. Centralizes DISABLE_CFI_DLSYM
  // annotations here to avoid spreading them across the codebase.
  DISABLE_CFI_DLSYM
  void InitDawnProcs(const DawnProcTable& procs) const {
    api_->InitDawnProcs(procs);
  }

  DISABLE_CFI_DLSYM
  void SetMetricsFns(const ChromeMLMetricsFns* fns) const {
    if (api_->SetMetricsFns) {
      api_->SetMetricsFns(fns);
    }
  }

  DISABLE_CFI_DLSYM
  void SetFatalErrorFn(ChromeMLFatalErrorFn error_fn) const {
    if (api_->SetFatalErrorFn) {
      api_->SetFatalErrorFn(error_fn);
    }
  }

  DISABLE_CFI_DLSYM
  ChromeMLSafetyResult ClassifyTextSafety(ChromeMLModel model,
                                          const char* text,
                                          float* scores,
                                          size_t* num_scores) const {
    return api_->ClassifyTextSafety(model, text, scores, num_scores);
  }

  DISABLE_CFI_DLSYM
  void DestroyModel(ChromeMLModel model) const { api_->DestroyModel(model); }

  DISABLE_CFI_DLSYM
  bool GetEstimatedPerformance(
      ChromeMLPerformanceInfo* performance_info) const {
    return api_->GetEstimatedPerformance(performance_info);
  }

  DISABLE_CFI_DLSYM
  bool QueryGPUAdapter(void (*adapter_callback_fn)(WGPUAdapter adpater,
                                                   void* userdata),
                       void* userdata) const {
    return api_->QueryGPUAdapter(adapter_callback_fn, userdata);
  }

  DISABLE_CFI_DLSYM
  bool GetCapabilities(PlatformFile file,
                       ChromeMLCapabilities& capabilities) const {
    if (api_->GetCapabilities) {
      return api_->GetCapabilities(file, capabilities);
    }
    return false;
  }

  DISABLE_CFI_DLSYM
  void SetFatalErrorNonGpuFn(ChromeMLFatalErrorFn error_fn) const {
    if (api_->SetFatalErrorNonGpuFn) {
      api_->SetFatalErrorNonGpuFn(error_fn);
    }
  }

  DISABLE_CFI_DLSYM
  ChromeMLModel SessionCreateModel(const ChromeMLModelDescriptor* descriptor,
                                   uintptr_t context,
                                   ChromeMLScheduleFn schedule) const {
    return api_->SessionCreateModel(descriptor, context, schedule);
  }

  DISABLE_CFI_DLSYM
  bool SessionAppend(ChromeMLSession session,
                     const ChromeMLAppendOptions* options,
                     ChromeMLCancel cancel) const {
    return api_->SessionAppend(session, options, cancel);
  }

  DISABLE_CFI_DLSYM
  bool SessionGenerate(ChromeMLSession session,
                       const ChromeMLGenerateOptions* options,
                       ChromeMLCancel cancel) const {
    return api_->SessionGenerate(session, options, cancel);
  }

  DISABLE_CFI_DLSYM
  bool SessionExecuteModel(ChromeMLSession session,
                           ChromeMLModel model,
                           const ChromeMLExecuteOptions* options,
                           ChromeMLCancel cancel) const {
    return api_->SessionExecuteModel(session, model, options, cancel);
  }

  DISABLE_CFI_DLSYM
  void SessionSizeInTokens(ChromeMLSession session,
                           const std::string& text,
                           const ChromeMLSizeInTokensFn& fn) const {
    api_->SessionSizeInTokens(session, text, fn);
  }

  DISABLE_CFI_DLSYM
  void SessionSizeInTokensInputPiece(ChromeMLSession session,
                                     ChromeMLModel model,
                                     const ml::InputPiece* input,
                                     size_t input_size,
                                     const ChromeMLSizeInTokensFn& fn) const {
    api_->SessionSizeInTokensInputPiece(session, model, input, input_size, fn);
  }

  DISABLE_CFI_DLSYM
  void SessionScore(ChromeMLSession session,
                    const std::string& text,
                    const ChromeMLScoreFn& fn) const {
    api_->SessionScore(session, text, fn);
  }

  DISABLE_CFI_DLSYM
  void SessionGetProbabilitiesBlocking(
      ChromeMLSession session,
      const std::string& input,
      const ChromeMLGetProbabilitiesBlockingFn& fn) const {
    api_->SessionGetProbabilitiesBlocking(session, input, fn);
  }

  DISABLE_CFI_DLSYM
  ChromeMLSession CreateSession(
      ChromeMLModel model,
      const ChromeMLAdaptationDescriptor* descriptor) const {
    return api_->CreateSession(model, descriptor);
  }

  DISABLE_CFI_DLSYM
  ChromeMLSession CloneSession(ChromeMLSession session) const {
    return api_->CloneSession(session);
  }

  DISABLE_CFI_DLSYM
  void DestroySession(ChromeMLSession session) const {
    api_->DestroySession(session);
  }

  DISABLE_CFI_DLSYM
  ChromeMLCancel CreateCancel() const { return api_->CreateCancel(); }

  DISABLE_CFI_DLSYM
  void DestroyCancel(ChromeMLCancel cancel) const {
    api_->DestroyCancel(cancel);
  }

  DISABLE_CFI_DLSYM
  void CancelExecuteModel(ChromeMLCancel cancel) const {
    api_->CancelExecuteModel(cancel);
  }

  DISABLE_CFI_DLSYM
  void SetConstraintFns(const ChromeMLConstraintFns* fns) const {
    if (api_->SetConstraintFns) {
      api_->SetConstraintFns(fns);
    }
  }

  // TODO (crbug.com/500473306): Remove the old GetTokenizerParams function and
  // rename the new one to GetTokenizerParams in the C-API.
  DISABLE_CFI_DLSYM
  bool GetTokenizerParams(ChromeMLModel model,
                          ChromeMLSession session,
                          const ChromeMLGetTokenizerParamsFn& fn) const {
    return api_->GetTokenizerParamsV2(model, session, fn);
  }

  bool HasCreateGpuDelegate() const {
    return api_->CreateGpuDelegate != nullptr;
  }

  bool HasDestroyGpuDelegate() const {
    return api_->DestroyGpuDelegate != nullptr;
  }

  DISABLE_CFI_DLSYM
  TfLiteDelegate* CreateGpuDelegate() const {
    if (api_->CreateGpuDelegate) {
      return api_->CreateGpuDelegate();
    }
    return nullptr;
  }

  DISABLE_CFI_DLSYM
  TfLiteDelegate* CreateGpuDelegateWithPrecision(
      GpuDelegatePrecision precision) const {
    if (api_->CreateGpuDelegateWithPrecision) {
      return api_->CreateGpuDelegateWithPrecision(precision);
    }
    return nullptr;
  }

  DISABLE_CFI_DLSYM
  void DestroyGpuDelegate(TfLiteDelegate* delegate) const {
    if (api_->DestroyGpuDelegate) {
      api_->DestroyGpuDelegate(delegate);
    }
  }

  // TS API methods
  DISABLE_CFI_DLSYM
  ChromeMLTSModel TSCreateModel(
      const ChromeMLTSModelDescriptor* descriptor) const {
    return api_->ts_api.CreateModel(descriptor);
  }

  DISABLE_CFI_DLSYM
  void TSDestroyModel(ChromeMLTSModel model) const {
    api_->ts_api.DestroyModel(model);
  }

  DISABLE_CFI_DLSYM
  ChromeMLSafetyResult TSClassifyTextSafety(ChromeMLTSModel model,
                                            const char* text,
                                            float* scores,
                                            size_t* num_scores) const {
    return api_->ts_api.ClassifyTextSafety(model, text, scores, num_scores);
  }

  // ASR API methods
  DISABLE_CFI_DLSYM
  ChromeMLASRStream ASRCreateStream(
      ChromeMLSession session,
      const ChromeMLASRStreamOptions* options) const {
    return api_->asr_api.CreateStream(session, options);
  }

  DISABLE_CFI_DLSYM
  void ASRAddAudioChunk(ChromeMLASRStream stream,
                        ml::AudioBuffer* audio_buffer) const {
    api_->asr_api.AddAudioChunk(stream, audio_buffer);
  }

  DISABLE_CFI_DLSYM
  void ASRDestroyStream(ChromeMLASRStream stream) const {
    api_->asr_api.DestroyStream(stream);
  }

 private:
  explicit ChromeML(std::unique_ptr<ChromeMLHolder> holder);
  explicit ChromeML(const ChromeMLAPI* api);

  static std::unique_ptr<ChromeML> Create(
      const std::optional<std::string>& library_name);

  std::unique_ptr<ChromeMLHolder> holder_;
  raw_ptr<const ChromeMLAPI> api_;
};

COMPONENT_EXPORT(ON_DEVICE_MODEL_ML)
const ChromeMLConstraintFns* GetConstraintFns();

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_H_
