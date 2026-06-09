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
  int num_active_clients = 0;
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
  model_with_buffer->model = tflite::FlatBufferModel::VerifyAndBuildFromBuffer(
      reinterpret_cast<const char*>(model_with_buffer->buffer.data()),
      model_with_buffer->buffer.size());

  if (!model_with_buffer->model) {
    LOG(ERROR) << "Failed to verify and build FlatBufferModel from buffer.";
    return nullptr;
  }
  return model_with_buffer;
}

class MlModelHandleImpl : public MlModelHandle {
 public:
  MlModelHandleImpl(tflite::FlatBufferModel* model,
                    base::OnceClosure on_destruction_closure)
      : model_(model),
        on_destruction_closure_(std::move(on_destruction_closure)) {
    CHECK(model);
  }
  ~MlModelHandleImpl() override {
    // Explicitly reset the model pointer as the pointed-to memory may be
    // affected by the destruction closure.
    model_ = nullptr;
    std::move(on_destruction_closure_).Run();
  }
  const tflite::FlatBufferModel* Get() override { return model_; }

 private:
  raw_ptr<tflite::FlatBufferModel> model_;
  base::OnceClosure on_destruction_closure_;
};

}  // namespace

class MlModelManagerImpl::ModelStorage {
 public:
  ModelStorage();
  ~ModelStorage();

  ModelStorage(const ModelStorage&) = delete;
  ModelStorage& operator=(const ModelStorage&) = delete;

  void SetModel(base::File tflite_file);
  void StopServingModel();
  std::unique_ptr<MlModelHandle> GetModel();
  void CancelModelLoadingTasks();
  bool HasPendingTasksForTesting() const;

 private:
  void OnModelHandleDestruction(ModelWithBuffer* model);
  void OnModelRead(std::unique_ptr<ModelWithBuffer> model_with_buffer);

  SEQUENCE_CHECKER(sequence_checker_);

  // The service can hold on to multiple models simultaneously. The models
  // travel between three storages:
  //
  // 1. `unused_serving_model_`: A newly loaded model is placed here. It's
  //    available for use but hasn't been requested yet.
  //
  // 2. `used_serving_model_`: When a client requests a model via
  //    `GetModel()`, the model is moved from `unused_serving_model_` to
  //    here. This indicates the model is actively in use. If all clients stop
  //    using it, it moves back to `unused_serving_model_`.
  //
  // 3. `retired_models_`: If a new model is loaded while the current one in
  //    `used_serving_model_` is still being used, the `used_serving_model_`
  //    is moved into this map. This keeps the model alive for existing
  //    clients while allowing new clients to use the new model.
  //
  // At most one of `unused_serving_model_` and `used_serving_model_` is
  // non-null at any given time.
  std::unique_ptr<ModelWithBuffer> unused_serving_model_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ModelWithBuffer> used_serving_model_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Maps ModelWithBuffer pointers to the respective full ModelWithBuffer
  // object, for easy lookup.
  absl::flat_hash_map<ModelWithBuffer*, std::unique_ptr<ModelWithBuffer>>
      retired_models_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<ModelStorage> weak_factory_{this};
};

MlModelManagerImpl::ModelStorage::ModelStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MlModelManagerImpl::ModelStorage::~ModelStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!used_serving_model_ && retired_models_.empty())
      << "ModelStorage has existing clients at destruction time";
}

void MlModelManagerImpl::ModelStorage::OnModelRead(
    std::unique_ptr<ModelWithBuffer> model_with_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model_with_buffer) {
    // Model load failed, ignore.
    return;
  }
  if (used_serving_model_) {
    retired_models_.emplace(used_serving_model_.get(),
                            std::move(used_serving_model_));
  }
  unused_serving_model_ = std::move(model_with_buffer);
}

void MlModelManagerImpl::ModelStorage::StopServingModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop any ongoing model loading: This stop signal makes it obsolete.
  CancelModelLoadingTasks();

  if (used_serving_model_) {
    retired_models_.emplace(used_serving_model_.get(),
                            std::move(used_serving_model_));
  }
  unused_serving_model_.reset();
  used_serving_model_.reset();
}

void MlModelManagerImpl::ModelStorage::SetModel(base::File tflite_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop loading any older models:
  // - They are soon replaced with this new file, and
  // - we don't want races due to different file operation durations.
  CancelModelLoadingTasks();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadModelContents, std::move(tflite_file)),
      base::BindOnce(&MlModelManagerImpl::ModelStorage::OnModelRead,
                     weak_factory_.GetWeakPtr()));
}

void MlModelManagerImpl::ModelStorage::OnModelHandleDestruction(
    ModelWithBuffer* model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(model);
  // Find the model, and update the client count.

  if (model == used_serving_model_.get()) {
    // The model is actively serving.
    CHECK_GT(used_serving_model_->num_active_clients, 0);
    --used_serving_model_->num_active_clients;
    if (used_serving_model_->num_active_clients == 0) {
      unused_serving_model_ = std::move(used_serving_model_);
    }
    return;
  }
  // If we get here, the model is one of the retired models.
  auto iter = retired_models_.find(model);
  CHECK(iter != retired_models_.end());
  ModelWithBuffer& retired_model = *(*iter).second;
  CHECK_GT(retired_model.num_active_clients, 0);
  --(retired_model.num_active_clients);
  if (retired_model.num_active_clients == 0) {
    // All clients are gone, the model can be deleted.
    retired_models_.erase(iter);
  }
}

std::unique_ptr<MlModelHandle> MlModelManagerImpl::ModelStorage::GetModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!unused_serving_model_ && !used_serving_model_) {
    return nullptr;
  }
  if (unused_serving_model_) {
    used_serving_model_ = std::move(unused_serving_model_);
  }
  ++(used_serving_model_->num_active_clients);
  return std::make_unique<MlModelHandleImpl>(
      used_serving_model_->model.get(),
      base::BindOnce(
          &MlModelManagerImpl::ModelStorage::OnModelHandleDestruction,
          // Safe because the MlModelManager API requires clients to
          // destroy their model handles within the manager lifetime.
          base::Unretained(this), used_serving_model_.get()));
}

void MlModelManagerImpl::ModelStorage::CancelModelLoadingTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
}

bool MlModelManagerImpl::ModelStorage::HasPendingTasksForTesting() const {
  return weak_factory_.HasWeakPtrs();
}

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

void MlModelManagerImpl::SetModel(mojom::MlModelType model_type,
                                  base::File tflite_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<ModelStorage>& storage = models_[model_type];
  if (!storage) {
    storage = std::make_unique<ModelStorage>();
  }
  storage->SetModel(std::move(tflite_file));
}

void MlModelManagerImpl::StopServingModel(mojom::MlModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = models_.find(model_type);
  if (it != models_.end()) {
    it->second->StopServingModel();
  }
}

std::unique_ptr<MlModelHandle> MlModelManagerImpl::GetModel(
    mojom::MlModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = models_.find(model_type);
  if (it == models_.end()) {
    return nullptr;
  }
  return it->second->GetModel();
}

bool MlModelManagerImpl::HasPendingTasksForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [_, storage] : models_) {
    if (storage->HasPendingTasksForTesting()) {
      return true;
    }
  }
  return false;
}

}  // namespace audio
