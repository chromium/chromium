// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_H_

#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace passage_embeddings {

class PassageEmbedder;

// Class implementation of the passage embeddings service mojo interface.
class PassageEmbeddingsService : public mojom::PassageEmbeddingsService {
 public:
  explicit PassageEmbeddingsService(
      mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver);
  PassageEmbeddingsService(const PassageEmbeddingsService&) = delete;
  PassageEmbeddingsService& operator=(const PassageEmbeddingsService) = delete;
  ~PassageEmbeddingsService() override;

 private:
  // mojom::PassageEmbeddingsService:
  void LoadModels(mojom::PassageEmbeddingsLoadModelsParamsPtr params,
                  mojo::PendingReceiver<mojom::PassageEmbedder> model,
                  LoadModelsCallback callback) override;

  mojo::Receiver<mojom::PassageEmbeddingsService> receiver_;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::unique_ptr<PassageEmbedder> embedder_;
#endif
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_H_
