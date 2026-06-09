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

  // Returns a handle for a TFLite model file of the specified type.
  // Returns nullptr if no valid model file is available.
  // The model shares its lifetime with the MlModelManager, and the client must
  // stop using and destroy the handle within that lifetime.
  virtual std::unique_ptr<MlModelHandle> GetModel(
      mojom::MlModelType model_type) = 0;
};

// Implementation of the MlModelManager interface. This class receives model
// information from the browser process through the mojom::MlModelManager Mojo
// interface and provides it to audio service components.
//
// Current Behavior:
// - Model files provided via SetModel() are loaded and cached for as long as
//   they are used or may be used.
// - GetModel() returns the last successfully loaded model.
// - StopServingModel() stops serving all previously set models, but the
//   models are kept alive for as long as existing clients need them.
class MlModelManagerImpl : public MlModelManager, public mojom::MlModelManager {
 public:
  MlModelManagerImpl();
  ~MlModelManagerImpl() override;

  MlModelManagerImpl(const MlModelManagerImpl&) = delete;
  MlModelManagerImpl& operator=(const MlModelManagerImpl&) = delete;

  void BindReceiver(mojo::PendingReceiver<mojom::MlModelManager> receiver);

  // mojom::MlModelManager implementation.
  void SetModel(mojom::MlModelType model_type, base::File tflite_file) override;
  void StopServingModel(mojom::MlModelType model_type) override;

  // MlModelManager implementation.
  std::unique_ptr<MlModelHandle> GetModel(
      mojom::MlModelType model_type) override;

  bool HasPendingTasksForTesting() const;

 private:
  class ModelStorage;

  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<mojo::Receiver<mojom::MlModelManager>> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  absl::flat_hash_map<mojom::MlModelType, std::unique_ptr<ModelStorage>> models_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_ML_MODEL_MANAGER_H_
