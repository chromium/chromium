// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder.h"

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "third_party/sentencepiece/src/src/sentencepiece_model.pb.h"

namespace {
// Number for threads to use for TFLite execution. -1 lets TFLite use the
// default number of threads.
constexpr int kNumThreads = -1;
}  // namespace

namespace passage_embeddings {

PassageEmbedder::PassageEmbedder(
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver)
    : receiver_(this, std::move(receiver)) {}

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

  if (!tflite_engine) {
    // Use default TFLite engine if not already passed in.
    tflite_engine = std::make_unique<tflite::task::core::TfLiteEngine>(
        std::make_unique<optimization_guide::TFLiteOpResolver>());
  }
  absl::Status model_load_status = tflite_engine->BuildModelFromFlatBuffer(
      reinterpret_cast<const char*>(embeddings_model_buffer_.data()),
      embeddings_model_buffer_.size());
  if (!model_load_status.ok()) {
    return false;
  }

  absl::Status interpreter_status = tflite_engine->InitInterpreter(kNumThreads);
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
    PassageEmbedder::GenerateEmbeddingsCallback callback) {
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  for (const std::string& input : inputs) {
    if (!sp_processor_ || !sp_processor_->status().ok()) {
      std::move(callback).Run({});
      return;
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
    base::UmaHistogramMediumTimes(
        "History.Embeddings.Embedder.TokenizationDuration",
        tokenize_timer.Elapsed());

    base::ElapsedTimer execute_timer;
    std::optional<std::vector<float>> embeddings = Execute(tokenized);
    base::UmaHistogramBoolean(
        "History.Embeddings.Embedder.EmbeddingsGenerationSucceeded",
        !!embeddings);
    if (!embeddings) {
      std::move(callback).Run({});
      return;
    }
    base::UmaHistogramMediumTimes(
        "History.Embeddings.Embedder.EmbeddingsGenerationDuration",
        execute_timer.Elapsed());

    mojom::PassageEmbeddingsResultPtr result =
        mojom::PassageEmbeddingsResult::New();
    result->embeddings = *embeddings;
    result->passage = input;
    results.push_back(std::move(result));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace passage_embeddings
