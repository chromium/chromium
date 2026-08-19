// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder_impl.h"

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "services/passage_embeddings/passage_embedder_executor.h"
#include "services/passage_embeddings/passage_embeddings_op_resolver.h"
#include "third_party/sentencepiece/src/src/sentencepiece_model.pb.h"
#include "third_party/sentencepiece/src/src/sentencepiece_processor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace {
// Records duration and trace event for embeddings generation.
void RecordEmbeddingsDurationMetrics(
    bool is_passive,
    bool execute_for_gemma,
    uint32_t signature_length,
    base::TimeTicks start_time,
    base::TimeDelta elapsed_time,
    std::optional<base::TimeDelta> elapsed_thread_time) {
  const perfetto::Track trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());

  if (execute_for_gemma) {
    TRACE_EVENT_BEGIN("loading", "SemanticEmbeddingsGeneration", trace_track,
                      start_time);
    std::string signature_str = "." + base::NumberToString(signature_length);
    if (elapsed_thread_time.has_value()) {
      base::UmaHistogramMediumTimes(
          "AI.SemanticEmbedder.PassageEmbeddingsGenerationThreadDuration" +
              signature_str,
          *elapsed_thread_time);
    }
    base::UmaHistogramMediumTimes(
        "AI.SemanticEmbedder.PassageEmbeddingsGenerationDuration" +
            signature_str,
        elapsed_time);
  } else {
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
            "History.Embeddings.Embedder."
            "QueryEmbeddingsGenerationThreadDuration",
            *elapsed_thread_time);
      }
      base::UmaHistogramMediumTimes(
          "History.Embeddings.Embedder.QueryEmbeddingsGenerationDuration",
          elapsed_time);
    }
  }

  TRACE_EVENT_END("loading", trace_track, start_time + elapsed_time);
}
}  // namespace

namespace passage_embeddings {

PassageEmbedderImpl::PassageEmbedderImpl(
    mojom::PassageEmbedderParamsPtr embedder_params)
    : execute_for_gemma_(embedder_params->execute_for_gemma),
      embeddings_cache_(embedder_params->embedder_cache_size),
      user_initiated_priority_num_threads_(
          embedder_params->user_initiated_priority_num_threads),
      urgent_priority_num_threads_(
          embedder_params->urgent_priority_num_threads),
      passive_priority_num_threads_(
          embedder_params->passive_priority_num_threads),
      allow_gpu_execution_(embedder_params->allow_gpu_execution) {}

PassageEmbedderImpl::~PassageEmbedderImpl() = default;

bool PassageEmbedderImpl::LoadModels(base::File embeddings_model_file,
                                     base::File sp_file,
                                     uint32_t embeddings_input_window_size) {
  UnloadModelFiles();

  if (!embeddings_model_file.IsValid() || !sp_file.IsValid()) {
    return false;
  }

  embeddings_model_file_ = std::move(embeddings_model_file);

  base::ElapsedTimer sp_timer;
  bool sp_load_success = LoadSentencePieceModelFile(std::move(sp_file));
  base::UmaHistogramBoolean(
      execute_for_gemma_
          ? "AI.SemanticEmbedder.SentencePieceModelLoadSucceeded"
          : "History.Embeddings.Embedder.SentencePieceModelLoadSucceeded",
      sp_load_success);
  if (!sp_load_success) {
    return false;
  }
  base::UmaHistogramMediumTimes(
      execute_for_gemma_
          ? "AI.SemanticEmbedder.SentencePieceModelLoadDuration"
          : "History.Embeddings.Embedder.SentencePieceModelLoadDuration",
      sp_timer.Elapsed());

  embeddings_input_window_size_ = embeddings_input_window_size;

  return true;
}

bool PassageEmbedderImpl::LoadSentencePieceModelFile(base::File sp_file) {
  base::MemoryMappedFile sp_model;
  bool was_mapped = sp_model.Initialize(std::move(sp_file));
  if (!was_mapped) {
    return false;
  }

  std::unique_ptr<sentencepiece::ModelProto> model_proto =
      std::make_unique<sentencepiece::ModelProto>();
  model_proto->ParseFromArray(sp_model.bytes().data(), sp_model.bytes().size());
  sp_processor_ = std::make_unique<sentencepiece::SentencePieceProcessor>();
  if (!(sp_processor_->Load(std::move(model_proto)).ok())) {
    sp_processor_.reset();
    return false;
  }
  return true;
}

bool PassageEmbedderImpl::BuildExecutionTask() {
  CHECK_NE(current_priority_, mojom::PassagePriority::kUnknown);
  executor_.reset();

  std::unique_ptr<tflite::OpResolver> op_resolver;
  if (execute_for_gemma_) {
    op_resolver = std::make_unique<GemmaOpResolver>();
  } else {
    op_resolver = std::make_unique<HistoryOpResolver>(allow_gpu_execution_);
  }

  std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine =
      std::make_unique<tflite::task::core::TfLiteEngine>(
          std::move(op_resolver));

  base::ElapsedTimer embeddings_timer;
#if BUILDFLAG(IS_WIN)
  absl::Status model_load_status = tflite_engine->BuildModelFromFileHandle(
      embeddings_model_file_.GetPlatformFile());
#else
  absl::Status model_load_status = tflite_engine->BuildModelFromFileDescriptor(
      embeddings_model_file_.GetPlatformFile());
#endif
  base::UmaHistogramBoolean(
      execute_for_gemma_
          ? "AI.SemanticEmbedder.EmbeddingsModelLoadSucceeded"
          : "History.Embeddings.Embedder.EmbeddingsModelLoadSucceeded",
      model_load_status.ok());
  if (!model_load_status.ok()) {
    return false;
  }
  base::UmaHistogramMediumTimes(
      execute_for_gemma_
          ? "AI.SemanticEmbedder.EmbeddingsModelLoadDuration"
          : "History.Embeddings.Embedder.EmbeddingsModelLoadDuration",
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

  if (execute_for_gemma_) {
    executor_ = std::make_unique<GemmaModelExecutor>(
        sp_processor_->bos_id(), sp_processor_->eos_id(),
        sp_processor_->pad_id(), std::move(tflite_engine));
  } else {
    executor_ = std::make_unique<HistoryModelExecutor>(
        embeddings_input_window_size_, sp_processor_->eos_id(),
        std::move(tflite_engine));
  }
  return true;
}

void PassageEmbedderImpl::UnloadModelFiles() {
  sp_processor_.reset();
  executor_.reset();
  embeddings_model_file_.Close();
}

std::optional<EmbedderExecutionResult> PassageEmbedderImpl::Execute(
    const std::vector<int>& input) {
  if (!executor_) {
    return std::nullopt;
  }
  return executor_->Execute(input);
}

std::vector<mojom::PassageEmbeddingsResultPtr>
PassageEmbedderImpl::GenerateEmbeddings(const std::vector<std::string>& inputs,
                                        mojom::PassagePriority priority) {
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  CHECK_NE(priority, mojom::PassagePriority::kUnknown);
  if (!sp_processor_ || !sp_processor_->status().ok()) {
    return results;
  }

  // Rebuild the execution task if necessary.
  if (current_priority_ != priority) {
    current_priority_ = priority;
    if (!BuildExecutionTask()) {
      current_priority_ = mojom::PassagePriority::kUnknown;
      return results;
    }
  }

  for (const std::string& input : inputs) {
    mojom::PassageEmbeddingsResultPtr result =
        mojom::PassageEmbeddingsResult::New();

    // OOM Cache Bypass: Only History Embeddings checks the cache.
    if (!execute_for_gemma_) {
      base::LRUCache<std::string, std::vector<float>>::iterator cache_value =
          embeddings_cache_.Get(input);
      bool cache_hit = cache_value != embeddings_cache_.end();
      base::UmaHistogramBoolean(kCacheHitMetricName, cache_hit);
      if (cache_hit) {
        result->embeddings = cache_value->second;
        results.push_back(std::move(result));
        continue;
      }
    }

    std::vector<int> tokenized;
    base::ElapsedTimer tokenize_timer;
    absl::Status status = sp_processor_->Encode(input, &tokenized);
    base::UmaHistogramBoolean(
        execute_for_gemma_
            ? "AI.SemanticEmbedder.TokenizationSucceeded"
            : "History.Embeddings.Embedder.TokenizationSucceeded",
        status.ok());
    if (!status.ok()) {
      return {};
    }
    base::UmaHistogramCounts1000(
        execute_for_gemma_ ? "AI.SemanticEmbedder.PassageTokenCount"
                           : "History.Embeddings.Embedder.PassageTokenCount",
        tokenized.size());
    if (!execute_for_gemma_) {
      base::UmaHistogramBoolean(
          "History.Embeddings.Embedder.InputTruncated",
          tokenized.size() > embeddings_input_window_size_);
    }
    base::TimeDelta tokenize_elapsed = tokenize_timer.Elapsed();
    base::UmaHistogramMediumTimes(
        execute_for_gemma_ ? "AI.SemanticEmbedder.TokenizationDuration"
                           : "History.Embeddings.Embedder.TokenizationDuration",
        tokenize_elapsed);

    const base::TimeTicks tokenize_start_time = tokenize_timer.start_time();
    const perfetto::Track trace_track =
        perfetto::Track(base::trace_event::GetNextGlobalTraceId());
    TRACE_EVENT_BEGIN("loading", "PassageTokenization", trace_track,
                      tokenize_start_time);
    TRACE_EVENT_END("loading", trace_track,
                    tokenize_start_time + tokenize_elapsed);

    base::ElapsedThreadTimer execute_thread_timer;
    base::ElapsedTimer execute_timer;
    std::optional<EmbedderExecutionResult> execution_result =
        Execute(tokenized);
    base::UmaHistogramBoolean(
        execute_for_gemma_
            ? "AI.SemanticEmbedder.EmbeddingsGenerationSucceeded"
            : "History.Embeddings.Embedder.EmbeddingsGenerationSucceeded",
        !!execution_result);
    if (!execution_result) {
      return {};
    }

    RecordEmbeddingsDurationMetrics(
        priority == mojom::PassagePriority::kPassive, execute_for_gemma_,
        execution_result->signature_length, execute_timer.start_time(),
        execute_timer.Elapsed(),
        execute_thread_timer.is_supported()
            ? std::optional<base::TimeDelta>(execute_thread_timer.Elapsed())
            : std::nullopt);

    // OOM Cache Bypass: Only save History Embeddings results to the cache.
    // Must Put() before std::move() to avoid storing an empty vector.
    if (!execute_for_gemma_) {
      embeddings_cache_.Put(input, execution_result->embeddings);
    }
    result->embeddings = std::move(execution_result->embeddings);

    results.push_back(std::move(result));
  }
  return results;
}

}  // namespace passage_embeddings
