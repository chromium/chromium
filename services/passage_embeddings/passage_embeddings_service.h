// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/updateable_sequenced_task_runner.h"
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
  void LoadModels(mojom::PassageEmbeddingsLoadModelsParamsPtr model_params,
                  mojom::PassageEmbedderParamsPtr embedder_params,
                  mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
                  LoadModelsCallback callback) override;

  // Called when the models have finished loading on the background thread.
  void OnModelsLoaded(LoadModelsCallback callback, bool success);

  mojo::Receiver<mojom::PassageEmbeddingsService> receiver_;

  // The task runner used for the embedder.
  scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner_;

  // Called when the embedder remote disconnects.
  void OnEmbedderDisconnect();

  std::unique_ptr<PassageEmbedder> embedder_;

  base::WeakPtrFactory<PassageEmbeddingsService> weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_H_
