// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_ML_MODEL_MANAGER_H_
#define SERVICES_AUDIO_ML_MODEL_MANAGER_H_

#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace audio {

// Holds the TFLite model and the buffer that backs it.
// The buffer must outlive the model.
struct ModelWithBuffer {
  explicit ModelWithBuffer(size_t buffer_size);
  ~ModelWithBuffer();

  std::vector<uint8_t> buffer;
  std::unique_ptr<tflite::FlatBufferModel> model;
};

// Interface for managing Machine Learning models within the audio service.
// This interface is used by components like the AudioProcessorHandler to access
// model information provided from the browser process via the
// mojom::MlModelManager interface.
class MlModelManager {
 public:
  virtual ~MlModelManager() = default;

  // Returns a pointer to the TFLite model file for the neural residual echo
  // estimator. Returns nullptr if no valid model file is set or loading fails.
  virtual raw_ptr<const tflite::FlatBufferModel>
  GetResidualEchoEstimationModel() = 0;
};

// Implementation of the MlModelManager interface. This class receives model
// information from the browser process through the mojom::MlModelManager Mojo
// interface and provides it to audio service components.
//
// Current Behavior:
// - A model file is provided via SetResidualEchoEstimationModel().
// - SetResidualEchoEstimationModel() is expected to be called exactly once with
// a valid file.
// - GetResidualEchoEstimationModel() will return the loaded model after it has
//   been read asynchronously.
class MlModelManagerImpl : public MlModelManager, public mojom::MlModelManager {
 public:
  MlModelManagerImpl();
  ~MlModelManagerImpl() override;

  MlModelManagerImpl(const MlModelManagerImpl&) = delete;
  MlModelManagerImpl& operator=(const MlModelManagerImpl&) = delete;

  void BindReceiver(mojo::PendingReceiver<mojom::MlModelManager> receiver);

  // mojom::MlModelManager implementation.
  void SetResidualEchoEstimationModel(base::File tflite_file) override;

  // MlModelManager implementation.
  raw_ptr<const tflite::FlatBufferModel> GetResidualEchoEstimationModel()
      override;

 private:
  void OnResidualEchoEstimationModelRead(
      std::unique_ptr<ModelWithBuffer> model_with_buffer);

  SEQUENCE_CHECKER(sequence_checker_);
  std::optional<mojo::Receiver<mojom::MlModelManager>> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Residual Echo Estimation model.
  std::unique_ptr<ModelWithBuffer> ree_model_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<MlModelManagerImpl> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_ML_MODEL_MANAGER_H_
