// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace passage_embeddings {

// Class implementation of the passage embedder mojo interface.
class PassageEmbedder : public mojom::PassageEmbedder {
 public:
  explicit PassageEmbedder(
      mojo::PendingReceiver<mojom::PassageEmbedder> receiver);
  PassageEmbedder(const PassageEmbedder&) = delete;
  PassageEmbedder& operator=(const PassageEmbedder) = delete;
  ~PassageEmbedder() override;

 private:
  // mojom::PassageEmbedder:
  void GenerateEmbeddings(const std::vector<std::string>& inputs,
                          GenerateEmbeddingsCallback callback) override;

  mojo::Receiver<mojom::PassageEmbedder> receiver_;
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_H_
