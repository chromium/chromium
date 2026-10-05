// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_ML_MODEL_MANAGER_H_
#define SERVICES_AUDIO_ML_MODEL_MANAGER_H_

#include <optional>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace media {
class MlModelHandle;
}  // namespace media

namespace audio {

// Interface for providing Machine Learning models within the audio service.
// This interface is used by components like the AudioProcessorHandler to access
// ML model information.
class MlModelManager {
 public:
  virtual ~MlModelManager() = default;

  // Returns a handle for a TFLite model of the specified type.
  // Returns nullptr if no model is currently available.
  // The returned model is ref-counted, thread-safe, and can safely outlive the
  // model manager.
  virtual scoped_refptr<media::MlModelHandle> GetModel(
      mojom::MlModelType model_type) = 0;

  // Reports that `failing_model` failed runtime component/interpreter
  // initialization. If it is still the model being served for `model_type`,
  // stops serving it so later GetModel() calls return nullptr until a new
  // model is set; otherwise (stale handle, unknown type, or null) no-op. Must
  // be called on the same sequence as GetModel().
  //
  // Invalidation is scoped to the lifetime of this audio service process; the
  // browser re-sends the model file when the service restarts.
  // TODO(crbug.com/512016773): Consider notifying the browser process (e.g.
  // AudioProcessMlModelForwarder) when a model is broken so that the file is
  // not re-sent if the audio utility process restarts.
  virtual void InvalidateModel(
      mojom::MlModelType model_type,
      scoped_refptr<media::MlModelHandle> failing_model) = 0;
};

// Implementation of the MlModelManager interface. This class receives model
// information from the browser process through the mojom::MlModelManager Mojo
// interface and provides it to audio service components.
//
// Current Behavior:
// - Model files provided to SetModel() are loaded and cached for serving.
// - GetModel() returns the last set and successfully loaded model.
// - InvalidateModel() stops serving a model that failed at runtime until the
//   next SetModel().
// - StopServingModel() stops GetModel() from serving any previously set model.
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
  scoped_refptr<media::MlModelHandle> GetModel(
      mojom::MlModelType model_type) override;
  void InvalidateModel(
      mojom::MlModelType model_type,
      scoped_refptr<media::MlModelHandle> failing_model) override;

  bool HasPendingTasksForTesting() const;

 private:
  class ServedModel;

  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<mojo::Receiver<mojom::MlModelManager>> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  absl::flat_hash_map<mojom::MlModelType, std::unique_ptr<ServedModel>> models_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_ML_MODEL_MANAGER_H_
