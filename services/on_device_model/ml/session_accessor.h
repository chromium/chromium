// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_SESSION_ACCESSOR_H_
#define SERVICES_ON_DEVICE_MODEL_ML_SESSION_ACCESSOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/constraint_factory.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace ml {

// Allows for safely accessing ChromeMLSession on a task runner. ChromeMLSession
// may make blocking calls, so it can't be used on the main thread.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) SessionAccessor {
 public:
  using Ptr = std::unique_ptr<SessionAccessor, base::OnTaskRunnerDeleter>;

  static Ptr Create(
      const ChromeML& chrome_ml,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ChromeMLModel model,
      on_device_model::mojom::SessionParamsPtr params,
      on_device_model::mojom::LoadAdaptationParamsPtr adaptation_params,
      std::optional<uint32_t> adaptation_id);

  ~SessionAccessor();

  // These methods forward to the relevant ChromeMLSession methods on the task
  // runner.
  Ptr Clone();
  ChromeMLCancelFn Append(const perfetto::Track& perfetto_id,
                          on_device_model::mojom::AppendOptionsPtr options,
                          ChromeMLContextSavedFn context_saved_fn);
  ChromeMLCancelFn Generate(
      const perfetto::Track& perfetto_id,
      on_device_model::mojom::GenerateOptionsPtr options,
      ConstraintFactory* constraint_factory,
      const std::optional<std::string>& model_response_prefix,
      ChromeMLExecutionOutputFn output_fn);
  void Score(const std::string& text, ChromeMLScoreFn score_fn);
  void GetProbabilitiesBlocking(const std::string& input,
                                ChromeMLGetProbabilitiesBlockingFn get_prob_fn);
  void SizeInTokens(on_device_model::mojom::InputPtr input,
                    ChromeMLSizeInTokensFn size_in_tokens_fn);
  void CreateAsrStream(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      const ChromeMLASRStreamOutputFn output_fn,
      base::OnceCallback<void(std::optional<on_device_model::mojom::AsrError>)>
          done_callback);
  void AsrAddAudioChunk(on_device_model::mojom::AudioDataPtr data);
  void Hint(on_device_model::mojom::HintOptionsPtr options,
            ConstraintFactory* constraint_factory);

 private:
  class Canceler;

  SessionAccessor(const ChromeML& chrome_ml,
                  scoped_refptr<base::SequencedTaskRunner> task_runner,
                  ChromeMLModel model);

  void CloneFrom(SessionAccessor* other);
  void CreateInternal(
      on_device_model::mojom::SessionParamsPtr params,
      on_device_model::mojom::LoadAdaptationParamsPtr adaptation_params,
      std::optional<uint32_t> adaptation_id);
  void AppendInternal(perfetto::Track perfetto_id,
                      on_device_model::mojom::AppendOptionsPtr append_options,
                      ChromeMLContextSavedFn context_saved_fn,
                      scoped_refptr<Canceler> canceler);
  void GenerateInternal(
      perfetto::Track perfetto_id,
      on_device_model::mojom::GenerateOptionsPtr generate_options,
      ConstraintFactory* constraint_factory,
      std::optional<std::string> model_response_prefix,
      ChromeMLExecutionOutputFn output_fn,
      scoped_refptr<Canceler> canceler);
  void ScoreInternal(const std::string& text, ChromeMLScoreFn score_fn);
  void GetProbabilitiesBlockingInternal(
      const std::string& input,
      ChromeMLGetProbabilitiesBlockingFn get_prob_fn);
  void SizeInTokensInternal(on_device_model::mojom::InputPtr input,
                            ChromeMLSizeInTokensFn size_in_tokens_fn);
  std::optional<on_device_model::mojom::AsrError> CreateAsrStreamInternal(
      on_device_model::mojom::AsrStreamOptionsPtr asr_options,
      const ChromeMLASRStreamOutputFn output_fn);
  void AsrAddAudioChunkInternal(on_device_model::mojom::AudioDataPtr data);
  void HintInternal(on_device_model::mojom::HintOptionsPtr options,
                    ConstraintFactory* constraint_factory);

  const raw_ref<const ChromeML> chrome_ml_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ChromeMLModel model_;
  ChromeMLSession session_ = 0;
  ChromeMLASRStream asr_stream_ = 0;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_SESSION_ACCESSOR_H_
