// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder.h"

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
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
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver)
    : receiver_(this, std::move(receiver)),
      embeddings_cache_(history_embeddings::kEmbedderCacheSize.Get()) {}

PassageEmbedder::~PassageEmbedder() = default;

bool PassageEmbedder::LoadModels(
    base::File* embeddings_model_file,
    base::File* sp_file,
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine) {
  UnloadModelFiles();

  base::ElapsedTimer embeddings_timer;
  bool embeddings_load_success =
      LoadEmbeddingsModelFile(embeddings_model_file, std::move(tflite_engine));
  base::UmaHistogramBoolean(
      "History.Embeddings.Embedder.EmbeddingsModelLoadSucceeded",
      embeddings_load_success);
  if (!embeddings_load_success) {
    return false;
  }
  base::UmaHistogramMediumTimes(
      "History.Embeddings.Embedder.EmbeddingsModelLoadDuration",
      embeddings_timer.Elapsed());

  base::ElapsedTimer sp_timer;
  bool sp_load_success = LoadSentencePieceModelFile(sp_file);
  base::UmaHistogramBoolean(
      "History.Embeddings.Embedder.SentencePieceModelLoadSucceeded",
      sp_load_success);
  if (!sp_load_success) {
    return false;
  }
  base::UmaHistogramMediumTimes(
      "History.Embeddings.Embedder.SentencePieceModelLoadDuration",
      sp_timer.Elapsed());

  return true;
}

void PassageEmbedder::SetEmbeddingsModelInputWindowSize(uint32_t size) {
  embeddings_input_window_size_ = size;
}

bool PassageEmbedder::LoadSentencePieceModelFile(base::File* sp_file) {
  auto sp_file_contents =
      base::HeapArray<uint8_t>::Uninit(sp_file->GetLength());
  std::optional<size_t> bytes_read = sp_file->Read(0, sp_file_contents);
  if (!bytes_read.has_value()) {
    return false;
  }

  auto model_proto = std::make_unique<sentencepiece::ModelProto>();
  model_proto->ParseFromArray(sp_file_contents.data(), sp_file_contents.size());
  sp_processor_ = std::make_unique<sentencepiece::SentencePieceProcessor>();
  if (!(sp_processor_->Load(std::move(model_proto)).ok())) {
    sp_processor_.reset();
    return false;
  }
  return true;
}

bool PassageEmbedder::LoadEmbeddingsModelFile(
    base::File* embeddings_file,
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine) {
  embeddings_model_buffer_ =
      base::HeapArray<uint8_t>::Uninit(embeddings_file->GetLength());
  std::optional<size_t> bytes_read =
      embeddings_file->Read(0, embeddings_model_buffer_);
  if (!bytes_read.has_value()) {
    return false;
  }

  tflite_engine_overridden_ = !!tflite_engine;
  override_tflite_engine_ = std::move(tflite_engine);
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
      std::make_unique<optimization_guide::TFLiteOpResolver>());

  absl::Status model_load_status = tflite_engine->BuildModelFromFlatBuffer(
      reinterpret_cast<const char*>(embeddings_model_buffer_.data()),
      embeddings_model_buffer_.size());
  if (!model_load_status.ok()) {
    return false;
  }

  int num_threads;
  switch (current_priority_) {
    case mojom::PassagePriority::kUserInitiated:
      num_threads = history_embeddings::kEmbedderNumThreads.Get();
      break;
    case mojom::PassagePriority::kPassive:
      num_threads = 1;
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
  embeddings_model_buffer_ = base::HeapArray<uint8_t>();
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
    result->passage = input;

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
