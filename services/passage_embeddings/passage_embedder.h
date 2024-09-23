// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_

#include "base/containers/heap_array.h"
#include "base/containers/lru_cache.h"
#include "base/files/file.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/passage_embedder_execution_task.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "third_party/sentencepiece/src/src/sentencepiece_processor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace passage_embeddings {

inline constexpr char kCacheHitMetricName[] =
    "History.Embeddings.Embedder.CacheHit";

// Class implementation of the passage embedder mojo interface.
class PassageEmbedder : public mojom::PassageEmbedder {
 public:
  explicit PassageEmbedder(
      mojo::PendingReceiver<mojom::PassageEmbedder> receiver);
  PassageEmbedder(const PassageEmbedder&) = delete;
  PassageEmbedder& operator=(const PassageEmbedder) = delete;
  ~PassageEmbedder() override;

  // Loads the given text embeddings model and the sentencepiece file for text
  // embedding generation. Return true if successful.
  //
  // A TfLiteEngine can be provided to override any defaults.
  bool LoadModels(base::File* embeddings_model_file,
                  base::File* sp_file,
                  std::unique_ptr<tflite::task::core::TfLiteEngine>
                      tflite_engine = nullptr);

  // Sets the input window size that the loaded embeddings model expects. Needs
  // to be called before the model can be executed.
  void SetEmbeddingsModelInputWindowSize(uint32_t size);

  // mojom::PassageEmbedder:
  void GenerateEmbeddings(const std::vector<std::string>& inputs,
                          mojom::PassagePriority priority,
                          GenerateEmbeddingsCallback callback) override;

 private:
  // Loads the text embeddings tflite model from the bytes in the given file.
  // Return true if successful.
  bool LoadEmbeddingsModelFile(
      base::File* embeddings_file,
      std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine);

  // Loads the sentencepiece model for tokenization, from the bytes in the given
  // file. Returns true if successful.
  bool LoadSentencePieceModelFile(base::File* sp_file);

  // Unloads all associated models.
  void UnloadModelFiles();

  // Builds a new execution task configured with the right number of threads
  // according to the priority. Replaces the old task if one exists. Returns
  // true on success.
  bool BuildExecutionTask();

  // Executes the model to generate text embeddings result for the input.
  std::optional<OutputType> Execute(InputType input);

  mojo::Receiver<mojom::PassageEmbedder> receiver_;

  std::unique_ptr<sentencepiece::SentencePieceProcessor> sp_processor_;

  std::unique_ptr<PassageEmbedderExecutionTask> loaded_model_;

  // Holds the bytes of the loaded text embedding model.
  base::HeapArray<uint8_t> embeddings_model_buffer_;

  // The input window size that the embeddings model expects.
  uint32_t embeddings_input_window_size_;

  // The priority that the active tflite_engine is set up for.
  mojom::PassagePriority current_priority_;

  // Whether the tflite engine has been overridden by caller during setup.
  bool tflite_engine_overridden_;

  // Temporarily stores the pointer to the override engine. Will be null when
  // it is loaded into an execution task.
  std::unique_ptr<tflite::task::core::TfLiteEngine> override_tflite_engine_;

  base::LRUCache<std::string, std::vector<float>> embeddings_cache_;
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_
