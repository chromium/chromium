// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder.h"

#include <utility>

#include "base/files/file.h"
#include "services/passage_embeddings/passage_embedder_impl.h"

namespace passage_embeddings {

PassageEmbedder::PassageEmbedder(
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
    mojom::PassageEmbedderParamsPtr embedder_params,
    base::OnceCallback<void()> on_disconnect,
    scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner)
    : receiver_(this, std::move(receiver)),
      task_runner_(std::move(task_runner)),
      internal_embedder_(task_runner_, std::move(embedder_params)) {
  receiver_.set_disconnect_handler(std::move(on_disconnect));
}

PassageEmbedder::~PassageEmbedder() = default;

void PassageEmbedder::LoadModels(
    base::File embeddings_model_file,
    base::File sp_file,
    uint32_t embeddings_input_window_size,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine) {
  internal_embedder_.AsyncCall(&PassageEmbedderImpl::LoadModels)
      .WithArgs(std::move(embeddings_model_file), std::move(sp_file),
                embeddings_input_window_size, std::move(tflite_engine))
      .Then(std::move(callback));
}

void PassageEmbedder::GenerateEmbeddings(const std::vector<std::string>& inputs,
                                         mojom::PassagePriority priority,
                                         GenerateEmbeddingsCallback callback) {
  CHECK_NE(priority, mojom::PassagePriority::kUnknown);
  task_counts_[priority]++;
  MaybeUpdatePriority();

  internal_embedder_.AsyncCall(&PassageEmbedderImpl::GenerateEmbeddings)
      .WithArgs(inputs, priority)
      .Then(base::BindOnce(&PassageEmbedder::OnGenerateEmbeddingsComplete,
                           weak_ptr_factory_.GetWeakPtr(), priority,
                           std::move(callback)));
}

void PassageEmbedder::OnGenerateEmbeddingsComplete(
    mojom::PassagePriority priority,
    GenerateEmbeddingsCallback callback,
    std::vector<mojom::PassageEmbeddingsResultPtr> results) {
  task_counts_[priority]--;
  MaybeUpdatePriority();

  std::move(callback).Run(std::move(results));
}

void PassageEmbedder::MaybeUpdatePriority() {
  base::TaskPriority new_priority;
  if (task_counts_[mojom::PassagePriority::kUrgent] > 0) {
    new_priority = base::TaskPriority::USER_BLOCKING;
  } else if (task_counts_[mojom::PassagePriority::kUserInitiated] > 0) {
    new_priority = base::TaskPriority::USER_VISIBLE;
  } else {
    new_priority = base::TaskPriority::BEST_EFFORT;
  }

  if (new_priority != current_priority_) {
    current_priority_ = new_priority;
    task_runner_->UpdatePriority(new_priority);
  }
}

}  // namespace passage_embeddings
