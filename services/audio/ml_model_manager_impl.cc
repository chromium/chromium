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
#include "media/webrtc/ml_model_handle.h"
#include "services/audio/ml_model_manager.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace audio {

namespace {

class MlModelHandleImpl : public media::MlModelHandle {
 public:
  MlModelHandleImpl(std::vector<uint8_t> model_buffer,
                    std::unique_ptr<tflite::FlatBufferModel> model)
      : model_buffer_(std::move(model_buffer)), model_(std::move(model)) {
    CHECK(model_);
  }

  const tflite::FlatBufferModel& Get() override { return *model_; }

 private:
  ~MlModelHandleImpl() override = default;

  // `model_buffer_` must outlive `model_`.
  const std::vector<uint8_t> model_buffer_;
  const std::unique_ptr<tflite::FlatBufferModel> model_;
};

// Reads the model contents from the given base::File.
// This function is intended to run on a background thread.
scoped_refptr<media::MlModelHandle> ReadModelContents(base::File model_file) {
  if (!model_file.IsValid()) {
    return nullptr;
  }
  int64_t length = model_file.GetLength();
  if (length <= 0) {
    return nullptr;
  }
  std::vector<uint8_t> buffer(length);
  if (!model_file.ReadAndCheck(0, buffer)) {
    return nullptr;
  }
  auto built_model = tflite::FlatBufferModel::VerifyAndBuildFromBuffer(
      reinterpret_cast<const char*>(buffer.data()), buffer.size());
  if (!built_model) {
    return nullptr;
  }
  return base::MakeRefCounted<MlModelHandleImpl>(std::move(buffer),
                                                 std::move(built_model));
}

}  // namespace

class MlModelManagerImpl::ServedModel {
 public:
  ServedModel();
  ~ServedModel();
  void Set(scoped_refptr<media::MlModelHandle> loaded_model);

 private:
  friend class MlModelManagerImpl;

  SEQUENCE_CHECKER(sequence_checker_);

  // The model currently being served, if available.
  scoped_refptr<media::MlModelHandle> model_;

  // Used to invalidate model file read tasks when new SetModel() or
  // StopServingModel() calls come in.
  base::WeakPtrFactory<ServedModel> weak_factory_{this};
};

MlModelManagerImpl::ServedModel::ServedModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MlModelManagerImpl::ServedModel::~ServedModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MlModelManagerImpl::ServedModel::Set(
    scoped_refptr<media::MlModelHandle> loaded_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loaded_model) {
    // Model loading failed.
    return;
  }
  model_ = std::move(loaded_model);
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
  std::unique_ptr<ServedModel>& served_model = models_[model_type];
  if (!served_model) {
    // No pre-existing `ServedModel`, we need to initialize the map entry.
    served_model = std::make_unique<ServedModel>();
  }
  // Stop loading any older models: they are soon replaced with this new file,
  // and we don't want races due to different file operation durations.
  served_model->weak_factory_.InvalidateWeakPtrs();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadModelContents, std::move(tflite_file)),
      base::BindOnce(&ServedModel::Set,
                     served_model->weak_factory_.GetWeakPtr()));
}

void MlModelManagerImpl::StopServingModel(mojom::MlModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  models_.erase(model_type);
}

scoped_refptr<media::MlModelHandle> MlModelManagerImpl::GetModel(
    mojom::MlModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = models_.find(model_type);
  if (it == models_.end()) {
    return nullptr;
  }
  return it->second->model_;
}

bool MlModelManagerImpl::HasPendingTasksForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [_, served_model] : models_) {
    if (served_model->weak_factory_.HasWeakPtrs()) {
      return true;
    }
  }
  return false;
}

}  // namespace audio
