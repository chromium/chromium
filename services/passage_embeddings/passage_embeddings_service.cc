// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

namespace passage_embeddings {

PassageEmbeddingsService::PassageEmbeddingsService(
    mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver)
    : receiver_(this, std::move(receiver)) {}

PassageEmbeddingsService::~PassageEmbeddingsService() = default;

void PassageEmbeddingsService::LoadModels(
    mojom::PassageEmbeddingsModelAssetsPtr assets,
    mojo::PendingReceiver<mojom::PassageEmbedder> model,
    LoadModelsCallback callback) {
  // Stub: returns success if the assets pointer is not null.
  std::move(callback).Run(!assets.is_null());
}

}  // namespace passage_embeddings
