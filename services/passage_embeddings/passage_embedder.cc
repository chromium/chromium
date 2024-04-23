// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder.h"

namespace passage_embeddings {

PassageEmbedder::PassageEmbedder(
    mojo::PendingReceiver<mojom::PassageEmbedder> receiver)
    : receiver_(this, std::move(receiver)) {}

PassageEmbedder::~PassageEmbedder() = default;

void PassageEmbedder::GenerateEmbeddings(
    const std::vector<std::string>& inputs,
    PassageEmbedder::GenerateEmbeddingsCallback callback) {
  // Stub: returns a vector of 10 values of 1.0 as embeddings for each input.
  std::vector<mojom::PassageEmbeddingsResultPtr> results;
  for (const std::string& input : inputs) {
    mojom::PassageEmbeddingsResultPtr result =
        mojom::PassageEmbeddingsResult::New();
    result->embeddings = std::vector<float>(10, 1.0f);
    result->passage = input;

    results.push_back(std::move(result));
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace passage_embeddings
