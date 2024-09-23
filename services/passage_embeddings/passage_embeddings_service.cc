// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include "base/files/file.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "services/passage_embeddings/passage_embedder.h"

namespace passage_embeddings {

PassageEmbeddingsService::PassageEmbeddingsService(
    mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver)
    : receiver_(this, std::move(receiver)) {}

PassageEmbeddingsService::~PassageEmbeddingsService() = default;

void PassageEmbeddingsService::LoadModels(
    mojom::PassageEmbeddingsLoadModelsParamsPtr params,
    mojo::PendingReceiver<mojom::PassageEmbedder> model,
    LoadModelsCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  embedder_ = std::make_unique<PassageEmbedder>(std::move(model));

  // Load the model files.
  if (params->input_window_size == 0 ||
      !embedder_->LoadModels(&params->embeddings_model, &params->sp_model)) {
    embedder_.reset();
    std::move(callback).Run(false);
    return;
  }
  embedder_->SetEmbeddingsModelInputWindowSize(params->input_window_size);

  std::move(callback).Run(true);
#else
  std::move(callback).Run(false);
#endif
}

}  // namespace passage_embeddings
