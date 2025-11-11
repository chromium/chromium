// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "services/audio/ml_model_manager.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace audio {

namespace {

// Reads the model contents from the given base::File.
// This function is intended to run on a background thread.
// Returns a struct containing the model and its backing buffer.
std::unique_ptr<ModelWithBuffer> ReadModelContents(base::File model_file) {
  int64_t length = model_file.GetLength();
  if (length <= 0) {
    LOG(ERROR) << "Invalid model file length.";
    return nullptr;
  }
  auto model_with_buffer = std::make_unique<ModelWithBuffer>(length);
  if (!model_file.ReadAndCheck(0, model_with_buffer->buffer)) {
    LOG(ERROR) << "Failed to read model file contents.";
    return nullptr;
  }
  model_with_buffer->model = tflite::FlatBufferModel::BuildFromBuffer(
      reinterpret_cast<const char*>(model_with_buffer->buffer.data()),
      model_with_buffer->buffer.size());

  if (!model_with_buffer->model) {
    LOG(ERROR) << "Failed to build FlatBufferModel from buffer.";
    return nullptr;
  }
  return model_with_buffer;
}

}  // namespace

ModelWithBuffer::ModelWithBuffer(size_t buffer_size) : buffer(buffer_size) {}
ModelWithBuffer::~ModelWithBuffer() = default;

MlModelManagerImpl::MlModelManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MlModelManagerImpl::~MlModelManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MlModelManagerImpl::BindReceiver(
    mojo::PendingReceiver<mojom::MlModelManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!receiver_.has_value());
  receiver_.emplace(this, std::move(receiver));
}

void MlModelManagerImpl::OnResidualEchoEstimationModelRead(
    std::unique_ptr<ModelWithBuffer> model_with_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ree_model_ = std::move(model_with_buffer);
}

void MlModelManagerImpl::SetResidualEchoEstimationModel(
    base::File tflite_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ree_model_ == nullptr);
  CHECK(tflite_file.IsValid());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadModelContents, std::move(tflite_file)),
      base::BindOnce(&MlModelManagerImpl::OnResidualEchoEstimationModelRead,
                     weak_factory_.GetWeakPtr()));
}

raw_ptr<const tflite::FlatBufferModel>
MlModelManagerImpl::GetResidualEchoEstimationModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ree_model_ ? ree_model_->model.get() : nullptr;
}

}  // namespace audio
