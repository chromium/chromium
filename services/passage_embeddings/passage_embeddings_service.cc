// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embeddings_service.h"

#include <utility>

#include "base/files/file.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "services/passage_embeddings/passage_embedder.h"
#endif

namespace passage_embeddings {

PassageEmbeddingsService::PassageEmbeddingsService(
    mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver)
    : receiver_(this, std::move(receiver)) {}

PassageEmbeddingsService::~PassageEmbeddingsService() = default;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PassageEmbeddingsService::OnEmbedderDisconnect() {
  embedder_.reset();
}
#endif

void PassageEmbeddingsService::LoadModels(
    mojom::PassageEmbeddingsLoadModelsParamsPtr model_params,
    mojom::PassageEmbedderParamsPtr embedder_params,
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
    LoadModelsCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
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
#else
  std::move(callback).Run(false);
#endif
}

}  // namespace passage_embeddings
