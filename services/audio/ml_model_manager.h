// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_ML_MODEL_MANAGER_H_
#define SERVICES_AUDIO_ML_MODEL_MANAGER_H_

#include <optional>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace audio {

struct ModelWithBuffer;

class MlModelHandle {
 public:
  virtual ~MlModelHandle() = default;

  // Returns a pointer to a TFLite model file, valid for the lifetime of the
  // Modelhandle.
  virtual const tflite::FlatBufferModel* Get() = 0;
};

// Interface for providing Machine Learning models within the audio service.
// This interface is used by components like the AudioProcessorHandler to access
// // ML model information.
class MlModelManager {
 public:
  virtual ~MlModelManager() = default;

  // Returns a handle for a TFLite model file for the neural residual echo
  // estimator. Returns nullptr if no valid model file is available.
  // The model shares its lifetime with the MlModelManager, and the client must
  // stop using and destroy the handle within that lifetime.
  virtual std::unique_ptr<MlModelHandle> GetResidualEchoEstimationModel() = 0;
};

// Implementation of the MlModelManager interface. This class receives model
// information from the browser process through the mojom::MlModelManager Mojo
// interface and provides it to audio service components.
//
// Current Behavior:
// - Model files provided via SetResidualEchoEstimationModel() are loaded and
//   cached for as long as they are used or may be used.
// - GetResidualEchoEstimationModel() returns the last successfully loaded
//   model.
// - StopServingResidualEchoEstimationModel() stops serving all previously set
//   models, but the models are kept alive for as long as existing clients need
//   them.
class MlModelManagerImpl : public MlModelManager, public mojom::MlModelManager {
 public:
  MlModelManagerImpl();
  ~MlModelManagerImpl() override;

  MlModelManagerImpl(const MlModelManagerImpl&) = delete;
  MlModelManagerImpl& operator=(const MlModelManagerImpl&) = delete;

  void BindReceiver(mojo::PendingReceiver<mojom::MlModelManager> receiver);

  // mojom::MlModelManager implementation.
  void SetResidualEchoEstimationModel(base::File tflite_file) override;
  void StopServingResidualEchoEstimationModel() override;

  // MlModelManager implementation.
  std::unique_ptr<MlModelHandle> GetResidualEchoEstimationModel() override;

  // Stops any ongoing loading of model files.
  void CancelModelLoadingTasks();

  bool HasPendingTasksForTesting() const;

 private:
  // Callback for MlModelHandle to track active clients for a model.
  void OnModelHandleDestruction(ModelWithBuffer* model_with_buffer);

  // Continuation for SetResidualEchoEstimationModel after work on background
  // thread after work on background thread.
  void OnResidualEchoEstimationModelRead(
      std::unique_ptr<ModelWithBuffer> model_with_buffer);

  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<mojo::Receiver<mojom::MlModelManager>> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The service can hold on to multiple models simultaneously. The models
  // travel between three storages:
  //
  // 1. `unused_serving_model_`: A newly loaded model is placed here. It's
  //    available for use but hasn't been requested yet.
  //
  // 2. `used_serving_model_`: When a client requests a model via
  //    `GetResidualEchoEstimationModel()`, the model is moved from
  //    `unused_serving_model_` to here. This indicates the model is actively
  //    in use. If all clients stop using it, it moves back to
  //    `unused_serving_model_`.
  //
  // 3. `retired_models_`: If a new model is loaded while the current one in
  //    `used_serving_model_` is still being used, the `used_serving_model_`
  //    is moved into this map. This keeps the model alive for existing clients
  //    while allowing new clients to use the new model.
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

  base::WeakPtrFactory<MlModelManagerImpl> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_ML_MODEL_MANAGER_H_
