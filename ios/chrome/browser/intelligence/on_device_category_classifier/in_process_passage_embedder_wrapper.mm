// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_passage_embedder_wrapper.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "components/passage_embeddings/core/passage_embeddings_features.h"

InProcessPassageEmbedderWrapper::InProcessPassageEmbedderWrapper(
    scoped_refptr<base::UpdateableSequencedTaskRunner> background_task_runner,
    base::RepeatingClosure on_disconnect_callback)
    : background_task_runner_(std::move(background_task_runner)),
      on_disconnect_callback_(std::move(on_disconnect_callback)) {}

InProcessPassageEmbedderWrapper::~InProcessPassageEmbedderWrapper() = default;

void InProcessPassageEmbedderWrapper::EnsurePassageEmbedder() {
  if (passage_embedder_remote_.is_bound()) {
    return;
  }
  passage_embedder_.reset();
  passage_embedder_remote_.reset();

  auto params = passage_embeddings::mojom::PassageEmbedderParams::New();
  params->execute_for_gemma = false;
  params->allow_gpu_execution = false;
  params->user_initiated_priority_num_threads = 4;
  params->urgent_priority_num_threads = 2;
  params->passive_priority_num_threads = 1;
  params->embedder_cache_size = passage_embeddings::kEmbedderCacheSize.Get();

  passage_embedder_ = std::make_unique<passage_embeddings::PassageEmbedder>(
      passage_embedder_remote_.BindNewPipeAndPassReceiver(), std::move(params),
      base::BindOnce(&InProcessPassageEmbedderWrapper::OnEmbedderDisconnect,
                     base::Unretained(this)),
      background_task_runner_);
  passage_embedder_remote_.set_disconnect_handler(
      base::BindOnce(&InProcessPassageEmbedderWrapper::OnEmbedderDisconnect,
                     base::Unretained(this)));
}

void InProcessPassageEmbedderWrapper::Reset() {
  passage_embedder_.reset();
  passage_embedder_remote_.reset();
}

void InProcessPassageEmbedderWrapper::LoadModels(base::File embeddings_file,
                                                 base::File sp_file,
                                                 uint32_t window_size,
                                                 LoadCallback callback) {
  EnsurePassageEmbedder();
  passage_embedder_->LoadModels(std::move(embeddings_file), std::move(sp_file),
                                window_size, std::move(callback));
}

void InProcessPassageEmbedderWrapper::GenerateEmbeddings(
    const std::vector<std::string>& passages,
    EmbeddingsCallback callback) {
  if (passage_embedder_remote_.is_bound() &&
      passage_embedder_remote_.is_connected()) {
    passage_embedder_remote_->GenerateEmbeddings(
        passages, passage_embeddings::mojom::PassagePriority::kPassive,
        std::move(callback));
  } else {
    std::move(callback).Run({});
  }
}

void InProcessPassageEmbedderWrapper::OnEmbedderDisconnect() {
  if (!passage_embedder_remote_.is_bound()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&InProcessPassageEmbedderWrapper::ResetAndNotifyDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InProcessPassageEmbedderWrapper::ResetAndNotifyDisconnect() {
  if (!passage_embedder_remote_.is_bound()) {
    return;
  }
  Reset();
  if (on_disconnect_callback_) {
    on_disconnect_callback_.Run();
  }
}
