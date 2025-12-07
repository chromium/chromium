// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "services/passage_embeddings/passage_embeddings_op_resolver.h"
#include "third_party/sentencepiece/src/src/sentencepiece_model.pb.h"

namespace {
// Records duration and trace event for embeddings generation.
void RecordEmbeddingsDurationMetrics(
    bool is_passive,
    base::TimeTicks start_time,
    base::TimeDelta elapsed_time,
    std::optional<base::TimeDelta> elapsed_thread_time) {
  const auto trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());

  if (is_passive) {
    TRACE_EVENT_BEGIN("loading", "PassageEmbeddingsGeneration", trace_track,
                      start_time);
    if (elapsed_thread_time.has_value()) {
      base::UmaHistogramMediumTimes(
          "History.Embeddings.Embedder."
          "PassageEmbeddingsGenerationThreadDuration",
          *elapsed_thread_time);
    }
    base::UmaHistogramMediumTimes(
        "History.Embeddings.Embedder.PassageEmbeddingsGenerationDuration",
        elapsed_time);
  } else {
    TRACE_EVENT_BEGIN("loading", "QueryEmbeddingsGeneration", trace_track,
                      start_time);
    if (elapsed_thread_time.has_value()) {
      base::UmaHistogramMediumTimes(
          "History.Embeddings.Embedder.QueryEmbeddingsGenerationThreadDuration",
          *elapsed_thread_time);
    }
    base::UmaHistogramMediumTimes(
        "History.Embeddings.Embedder.QueryEmbeddingsGenerationDuration",
        elapsed_time);
  }

  TRACE_EVENT_END("loading", trace_track, start_time + elapsed_time);
}
}  // namespace

namespace passage_embeddings {

PassageEmbedder::PassageEmbedder(
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
    mojom::PassageEmbedderParamsPtr embedder_params,
    base::OnceCallback<void()> on_disconnect)
    : receiver_(this, std::move(receiver)),
      embeddings_cache_(embedder_params->embedder_cache_size),
      user_initiated_priority_num_threads_(
          embedder_params->user_initiated_priority_num_threads),
      urgent_priority_num_threads_(
          embedder_params->urgent_priority_num_threads),
      passive_priority_num_threads_(
          embedder_params->passive_priority_num_threads),
      allow_gpu_execution_(embedder_params->allow_gpu_execution) {
  receiver_.set_disconnect_handler(std::move(on_disconnect));
}

PassageEmbedder::~PassageEmbedder() = default;

bool PassageEmbedder::LoadModels(
    base::File embeddings_model_file,
    base::File sp_file,
    uint32_t embeddings_input_window_size,
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine) {
  UnloadModelFiles();

  embeddings_model_file_ = std::move(embeddings_model_file);

  tflite_engine_overridden_ = !!tflite_engine;
  override_tflite_engine_ = std::move(tflite_engine);

  base::ElapsedTimer sp_timer;
  bool sp_load_success = LoadSentencePieceModelFile(std::move(sp_file));
  base::UmaHistogramBoolean(
      "History.Embeddings.Embedder.SentencePieceModelLoadSucceeded",
      sp_load_success);
  if (!sp_load_success) {
    return false;
  }
  base::UmaHistogramMediumTimes(
      "History.Embeddings.Embedder.SentencePieceModelLoadDuration",
      sp_timer.Elapsed());

  embeddings_input_window_size_ = embeddings_input_window_size;

  return true;
}

bool PassageEmbedder::LoadSentencePieceModelFile(base::File sp_file) {
  base::MemoryMappedFile sp_model;
  bool was_mapped = sp_model.Initialize(std::move(sp_file));
  if (!was_mapped) {
    return false;
  }

  auto model_proto = std::make_unique<sentencepiece::ModelProto>();
  model_proto->ParseFromArray(sp_model.data(), sp_model.length());
  sp_processor_ = std::make_unique<sentencepiece::SentencePieceProcessor>();
  if (!(sp_processor_->Load(std::move(model_proto)).ok())) {
    sp_processor_.reset();
    return false;
  }
  return true;
}

bool PassageEmbedder::BuildExecutionTask() {
  CHECK_NE(current_priority_, mojom::PassagePriority::kUnknown);
  // Do nothing if an override model has been loaded.
  if (tflite_engine_overridden_ && !override_tflite_engine_) {
    return true;
  }

  loaded_model_.reset();

  // Load the override model if it is set but not loaded yet.
  if (tflite_engine_overridden_) {
    loaded_model_ = std::make_unique<PassageEmbedderExecutionTask>(
        std::move(override_tflite_engine_));
    override_tflite_engine_.reset();
    return true;
  }

  // Build a new task from the model bytes and the task priority.
  auto tflite_engine = std::make_unique<tflite::task::core::TfLiteEngine>(
      std::make_unique<PassageEmbeddingsOpResolver>(allow_gpu_execution_));

  base::ElapsedTimer embeddings_timer;
#if BUILDFLAG(IS_WIN)
  absl::Status model_load_status = tflite_engine->BuildModelFromFileHandle(
      embeddings_model_file_.GetPlatformFile());
#else
  absl::Status model_load_status = tflite_engine->BuildModelFromFileDescriptor(
      embeddings_model_file_.GetPlatformFile());
#endif
  base::UmaHistogramBoolean(
      "History.Embeddings.Embedder.EmbeddingsModelLoadSucceeded",
      model_load_status.ok());
  if (!model_load_status.ok()) {
    return false;
  }
  base::UmaHistogramMediumTimes(
      "History.Embeddings.Embedder.EmbeddingsModelLoadDuration",
      embeddings_timer.Elapsed());

  int num_threads;
  switch (current_priority_) {
    case mojom::PassagePriority::kUserInitiated:
      num_threads = user_initiated_priority_num_threads_;
      break;
    case mojom::PassagePriority::kUrgent:
      num_threads = urgent_priority_num_threads_;
      break;
    case mojom::PassagePriority::kPassive:
      num_threads = passive_priority_num_threads_;
      break;
    case mojom::PassagePriority::kUnknown:
      return false;
  }

  absl::Status interpreter_status = tflite_engine->InitInterpreter(num_threads);
  if (!interpreter_status.ok()) {
    return false;
  }

  loaded_model_ =
      std::make_unique<PassageEmbedderExecutionTask>(std::move(tflite_engine));

  return true;
}

void PassageEmbedder::UnloadModelFiles() {
  sp_processor_.reset();
  loaded_model_.reset();
  embeddings_model_file_.Close();
}

std::optional<OutputType> PassageEmbedder::Execute(InputType input) {
  if (!loaded_model_) {
    return std::nullopt;
  }
  return loaded_model_->Execute(input);
}

void PassageEmbedder::GenerateEmbeddings(
    const std::vector<std::string>& inputs,
    mojom::PassagePriority priority,
    PassageEmbedder::GenerateEmbeddingsCallback callback) {
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  CHECK_NE(priority, mojom::PassagePriority::kUnknown);
  if (!sp_processor_ || !sp_processor_->status().ok()) {
    std::move(callback).Run({});
    return;
  }

  // Rebuild the execution task if necessary.
  if (current_priority_ != priority) {
    current_priority_ = priority;
    BuildExecutionTask();
  }

  for (const std::string& input : inputs) {
    mojom::PassageEmbeddingsResultPtr result =
        mojom::PassageEmbeddingsResult::New();

    auto cache_value = embeddings_cache_.Get(input);
    bool cache_hit = cache_value != embeddings_cache_.end();
    base::UmaHistogramBoolean(kCacheHitMetricName, cache_hit);
    if (cache_hit) {
      result->embeddings = cache_value->second;
      results.push_back(std::move(result));
      continue;
    }

    std::vector<int> tokenized;
    base::ElapsedTimer tokenize_timer;
    auto status = sp_processor_->Encode(input, &tokenized);
    base::UmaHistogramBoolean(
        "History.Embeddings.Embedder.TokenizationSucceeded", status.ok());
    if (!status.ok()) {
      std::move(callback).Run({});
      return;
    }
    base::UmaHistogramCounts1000(
        "History.Embeddings.Embedder.PassageTokenCount", tokenized.size());
    if (tokenized.size() < embeddings_input_window_size_) {
      tokenized.push_back(sp_processor_->eos_id());
    }
    base::UmaHistogramBoolean("History.Embeddings.Embedder.InputTruncated",
                              tokenized.size() > embeddings_input_window_size_);
    tokenized.resize(embeddings_input_window_size_);
    base::TimeDelta tokenize_elapsed = tokenize_timer.Elapsed();
    base::UmaHistogramMediumTimes(
        "History.Embeddings.Embedder.TokenizationDuration", tokenize_elapsed);

    const auto tokenize_start_time = tokenize_timer.start_time();
    const auto trace_track =
        perfetto::Track(base::trace_event::GetNextGlobalTraceId());
    TRACE_EVENT_BEGIN("loading", "PassageTokenization", trace_track,
                      tokenize_start_time);
    TRACE_EVENT_END("loading", trace_track,
                    tokenize_start_time + tokenize_elapsed);

    base::ElapsedThreadTimer execute_thread_timer;
    base::ElapsedTimer execute_timer;
    std::optional<std::vector<float>> embeddings = Execute(tokenized);
    base::UmaHistogramBoolean(
        "History.Embeddings.Embedder.EmbeddingsGenerationSucceeded",
        !!embeddings);
    if (!embeddings) {
      std::move(callback).Run({});
      return;
    }

    RecordEmbeddingsDurationMetrics(
        priority == mojom::PassagePriority::kPassive,
        execute_timer.start_time(), execute_timer.Elapsed(),
        execute_thread_timer.is_supported()
            ? std::optional<base::TimeDelta>(execute_thread_timer.Elapsed())
            : std::nullopt);

    result->embeddings = *embeddings;
    embeddings_cache_.Put({input, *embeddings});

    results.push_back(std::move(result));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace passage_embeddings
