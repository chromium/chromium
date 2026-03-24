// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include <utility>

#include "base/files/file.h"
#include "services/passage_embeddings/passage_embedder.h"

namespace passage_embeddings {

PassageEmbeddingsService::PassageEmbeddingsService(
    mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver)
    : receiver_(this, std::move(receiver)) {}

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
                     base::Unretained(this)));

  // Load the model files.
  if (model_params->input_window_size == 0 ||
      !embedder_->LoadModels(std::move(model_params->embeddings_model),
                             std::move(model_params->sp_model),
                             model_params->input_window_size)) {
    embedder_.reset();
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

}  // namespace passage_embeddings
