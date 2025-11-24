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

// Holds the TFLite model and the buffer that backs it.
// The buffer must outlive the model.
struct ModelWithBuffer {
  explicit ModelWithBuffer(size_t buffer_size) : buffer(buffer_size) {}
  ~ModelWithBuffer() = default;

  std::vector<uint8_t> buffer;
  std::unique_ptr<tflite::FlatBufferModel> model;
};

namespace {

// Reads the model contents from the given base::File.
// This function is intended to run on a background thread.
// Returns a struct containing the model and its backing buffer.
std::unique_ptr<ModelWithBuffer> ReadModelContents(base::File model_file) {
  if (!model_file.IsValid()) {
    LOG(ERROR) << "Invalid model file.";
    return nullptr;
  }
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
  if (!model_with_buffer) {
    // Model load failed, ignore.
    return;
  }

  // Only a single update is expected.
  CHECK(!serving_model_ && !retired_model_);
  serving_model_ = std::move(model_with_buffer);
}

void MlModelManagerImpl::StopServingResidualEchoEstimationModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop any ongoing model loading: This stop signal makes it obsolete.
  weak_factory_.InvalidateWeakPtrs();

  retired_model_ = std::move(serving_model_);
}

void MlModelManagerImpl::SetResidualEchoEstimationModel(
    base::File tflite_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only a single update is expected.
  CHECK(!serving_model_ && !retired_model_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadModelContents, std::move(tflite_file)),
      base::BindOnce(&MlModelManagerImpl::OnResidualEchoEstimationModelRead,
                     weak_factory_.GetWeakPtr()));
}

raw_ptr<const tflite::FlatBufferModel>
MlModelManagerImpl::GetResidualEchoEstimationModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return serving_model_ ? serving_model_->model.get() : nullptr;
}

bool MlModelManagerImpl::HasPendingTasksForTesting() const {
  return weak_factory_.HasWeakPtrs();
}

}  // namespace audio
