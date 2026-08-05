// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_PASSAGE_EMBEDDER_WRAPPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_PASSAGE_EMBEDDER_WRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#import "base/files/file.h"
#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/task/updateable_sequenced_task_runner.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "services/passage_embeddings/passage_embedder.h"
#import "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

// Wraps passage_embeddings::PassageEmbedder and its mojo::Remote, handling
// initialization parameters, pipe binding, and connection resets.
class InProcessPassageEmbedderWrapper {
 public:
  using LoadCallback = base::OnceCallback<void(bool success)>;
  using EmbeddingsCallback = base::OnceCallback<void(
      std::vector<passage_embeddings::mojom::PassageEmbeddingsResultPtr>)>;

  InProcessPassageEmbedderWrapper(
      scoped_refptr<base::UpdateableSequencedTaskRunner> background_task_runner,
      base::RepeatingClosure on_disconnect_callback);
  ~InProcessPassageEmbedderWrapper();

  InProcessPassageEmbedderWrapper(const InProcessPassageEmbedderWrapper&) =
      delete;
  InProcessPassageEmbedderWrapper& operator=(
      const InProcessPassageEmbedderWrapper&) = delete;

  // Ensures that the underlying PassageEmbedder service is created and bound.
  void EnsurePassageEmbedder();

  // Resets the embedder and remote connection.
  void Reset();

  // Loads models into the passage embedder.
  void LoadModels(base::File embeddings_file,
                  base::File sp_file,
                  uint32_t window_size,
                  LoadCallback callback);

  // Generates embeddings for `passages` using the passive priority queue.
  void GenerateEmbeddings(const std::vector<std::string>& passages,
                          EmbeddingsCallback callback);

 private:
  // Analogous to `PassageEmbeddingsService::OnEmbedderDisconnect` on Desktop.
  void OnEmbedderDisconnect();
  void ResetAndNotifyDisconnect();

  scoped_refptr<base::UpdateableSequencedTaskRunner> background_task_runner_;
  base::RepeatingClosure on_disconnect_callback_;
  std::unique_ptr<passage_embeddings::PassageEmbedder> passage_embedder_;
  mojo::Remote<passage_embeddings::mojom::PassageEmbedder>
      passage_embedder_remote_;
  base::WeakPtrFactory<InProcessPassageEmbedderWrapper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_IN_PROCESS_PASSAGE_EMBEDDER_WRAPPER_H_
