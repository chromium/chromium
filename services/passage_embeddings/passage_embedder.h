// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/base_task_api.h"

namespace passage_embeddings {

class PassageEmbedderImpl;

// Class implementation of the passage embedder mojo interface.
class PassageEmbedder : public mojom::PassageEmbedder {
 public:
  PassageEmbedder(
      mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
      mojom::PassageEmbedderParamsPtr embedder_params,
      base::OnceCallback<void()> on_disconnect,
      scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner);
  PassageEmbedder(const PassageEmbedder&) = delete;
  PassageEmbedder& operator=(const PassageEmbedder) = delete;
  ~PassageEmbedder() override;

  // Loads the given text embeddings model and the sentencepiece file for text
  // embedding generation. Invokes `callback` with the success state.
  //
  // A TfLiteEngine can be provided to override any defaults.
  void LoadModels(base::File embeddings_model_file,
                  base::File sp_file,
                  uint32_t embeddings_input_window_size,
                  base::OnceCallback<void(bool)> callback,
                  std::unique_ptr<tflite::task::core::TfLiteEngine>
                      tflite_engine = nullptr);

  // mojom::PassageEmbedder:
  void GenerateEmbeddings(const std::vector<std::string>& inputs,
                          mojom::PassagePriority priority,
                          GenerateEmbeddingsCallback callback) override;

 private:
  void OnGenerateEmbeddingsComplete(
      mojom::PassagePriority priority,
      GenerateEmbeddingsCallback callback,
      std::vector<mojom::PassageEmbeddingsResultPtr> results);

  // Updates the priority of the task runner based on the current number of
  // active tasks of each priority.
  void MaybeUpdatePriority();

  mojo::Receiver<mojom::PassageEmbedder> receiver_;

  scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner_;

  base::SequenceBound<PassageEmbedderImpl> internal_embedder_;

  base::flat_map<mojom::PassagePriority, int> task_counts_;

  base::TaskPriority current_priority_ = base::TaskPriority::BEST_EFFORT;

  base::WeakPtrFactory<PassageEmbedder> weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_
