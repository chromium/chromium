// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_IMPL_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/file.h"
#include "services/passage_embeddings/passage_embedder_execution_task.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "third_party/sentencepiece/src/src/sentencepiece_processor.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace passage_embeddings {

inline constexpr char kCacheHitMetricName[] =
    "History.Embeddings.Embedder.CacheHit";

// The actual implementation of the passage embedder, which is intended to run
// on a background sequence.
class PassageEmbedderImpl {
 public:
  explicit PassageEmbedderImpl(mojom::PassageEmbedderParamsPtr embedder_params);
  PassageEmbedderImpl(const PassageEmbedderImpl&) = delete;
  PassageEmbedderImpl& operator=(const PassageEmbedderImpl) = delete;
  ~PassageEmbedderImpl();

  // Loads the given text embeddings model and the sentencepiece file for text
  // embedding generation. Return true if successful.
  bool LoadModels(base::File embeddings_model_file,
                  base::File sp_file,
                  uint32_t embeddings_input_window_size);

  // Executes the model to generate text embeddings result for the input.
  std::vector<mojom::PassageEmbeddingsResultPtr> GenerateEmbeddings(
      const std::vector<std::string>& inputs,
      mojom::PassagePriority priority);

 private:
  // Loads the sentencepiece model for tokenization, from the bytes in the given
  // file. Returns true if successful.
  bool LoadSentencePieceModelFile(base::File sp_file);

  // Unloads all associated models.
  void UnloadModelFiles();

  // Builds a new execution task configured with the right number of threads
  // according to the priority. Replaces the old task if one exists. Returns
  // true on success.
  bool BuildExecutionTask();

  // Executes the model to generate text embeddings result for the input.
  std::optional<OutputType> Execute(InputType input);

  std::unique_ptr<sentencepiece::SentencePieceProcessor> sp_processor_;

  std::unique_ptr<PassageEmbedderExecutionTask> loaded_model_;

  // The text embedding model file. Empty when not loaded.
  base::File embeddings_model_file_;

  // The input window size that the embeddings model expects.
  uint32_t embeddings_input_window_size_;

  // The priority that the active tflite_engine is set up for.
  mojom::PassagePriority current_priority_ = mojom::PassagePriority::kUnknown;

  base::LRUCache<std::string, std::vector<float>> embeddings_cache_;

  // The number of threads to use for PassagePriority::kUserInitiated.
  uint32_t user_initiated_priority_num_threads_;

  // The number of threads to use for PassagePriority::kUrgent.
  uint32_t urgent_priority_num_threads_;

  // The number of threads to use for PassagePriority::kPassive.
  uint32_t passive_priority_num_threads_;

  // Whether to allow model execution to run on the GPU if available for the
  // device.
  bool allow_gpu_execution_ = false;
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_IMPL_H_
