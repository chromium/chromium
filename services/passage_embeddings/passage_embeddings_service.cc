// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include <utility>

#include "base/files/file.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "services/passage_embeddings/passage_embedder.h"

namespace passage_embeddings {

PassageEmbeddingsService::PassageEmbeddingsService(
    mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver)
    : receiver_(this, std::move(receiver)),
      task_runner_(base::ThreadPool::CreateUpdateableSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           // MUST_USE_FOREGROUND is used to ensure that when the priority is
           // increased from BEST_EFFORT, the change takes effect for the
           // currently running task, preventing it from being descheduled in
           // favor of other higher priority work on the system.
           base::ThreadPolicy::MUST_USE_FOREGROUND})) {}

PassageEmbeddingsService::~PassageEmbeddingsService() = default;

void PassageEmbeddingsService::OnEmbedderDisconnect() {
  embedder_.reset();
}

void PassageEmbeddingsService::LoadModels(
    mojom::PassageEmbeddingsLoadModelsParamsPtr model_params,
    mojom::PassageEmbedderParamsPtr embedder_params,
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
    LoadModelsCallback callback) {
  embedder_ = std::make_unique<PassageEmbedder>(
      std::move(receiver), std::move(embedder_params),
      base::BindOnce(&PassageEmbeddingsService::OnEmbedderDisconnect,
                     base::Unretained(this)),
      task_runner_);

  if (model_params->input_window_size == 0) {
    embedder_.reset();
    std::move(callback).Run(false);
    return;
  }

  embedder_->LoadModels(
      std::move(model_params->embeddings_model),
      std::move(model_params->sp_model), model_params->input_window_size,
      base::BindOnce(&PassageEmbeddingsService::OnModelsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PassageEmbeddingsService::OnModelsLoaded(LoadModelsCallback callback,
                                              bool success) {
  if (!success) {
    embedder_.reset();
  }
  std::move(callback).Run(success);
}

}  // namespace passage_embeddings
