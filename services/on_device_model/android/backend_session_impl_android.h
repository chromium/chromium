// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_SESSION_IMPL_ANDROID_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_SESSION_IMPL_ANDROID_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/android/sequence_checker_helper.h"
#include "services/on_device_model/backend_session.h"

namespace on_device_model {

// Android implementation of BackendSession. A Java counterpart with the same
// lifetime will be created when this object is created. The model may be loaded
// in memory as soon as this object is created.
class BackendSessionImplAndroid : public BackendSession {
 public:
  // The result of a generate call.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.on_device_model
  enum class GenerateResult {
    kSuccess = 0,
    kUnknownError = 1,
    // The backend API is not constructed. This happens if this is an upstream
    // build.
    kApiNotAvailable = 2,
    // The backend is not able to find the feature ID. This can happen if AICore
    // doesn't enable the feature as part of experiments or device filters.
    kFeatureIsNull = 3,
    // An exception is thrown when getting the feature. This can happen if the
    // AICore APK is not installed on the device.
    kGetFeatureError = 4,
    // A general exception is thrown when running inference.
    kInferenceGeneralError = 5,
    // A request processing error is thrown when running inference. This may
    // be caused by safety filtering.
    kInferenceRequestProcessingError = 6,
    // A response processing error is thrown when running inference. This may
    // be caused by safety filtering.
    kInferenceResponseProcessingError = 7,
    kMaxValue = kInferenceResponseProcessingError,
  };

  BackendSessionImplAndroid(
      optimization_guide::proto::ModelExecutionFeature feature,
      on_device_model::mojom::SessionParamsPtr params);
  ~BackendSessionImplAndroid() override;

  // BackendSession:
  void Append(on_device_model::mojom::AppendOptionsPtr options,
              mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
              base::OnceClosure on_complete) override;
  void Generate(
      on_device_model::mojom::GenerateOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response,
      base::OnceClosure on_complete) override;
  void SizeInTokens(on_device_model::mojom::InputPtr input,
                    base::OnceCallback<void(uint32_t)> callback) override;
  void Score(const std::string& text,
             base::OnceCallback<void(float)> callback) override;
  void GetProbabilitiesBlocking(
      const std::string& input,
      base::OnceCallback<void(const std::vector<float>&)> callback) override;
  std::unique_ptr<BackendSession> Clone() override;
  void AsrStream(on_device_model::mojom::AsrStreamOptionsPtr options,
                 mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder>
                     response) override;
  void AsrAddAudioChunk(on_device_model::mojom::AudioDataPtr data) override;

  // Called by Java (can be called on any thread):
  // Called when the response of `Generate` is received from the AiCoreSession.
  void OnResponse(const std::string& response);
  // Called when the response of `Generate` is completed from the AiCoreSession.
  void OnComplete(GenerateResult generate_result);

 private:
  BackendSessionImplAndroid(
      optimization_guide::proto::ModelExecutionFeature feature,
      on_device_model::mojom::SessionParamsPtr params,
      const std::vector<ml::InputPiece>& context_input_pieces);

  void OnResponseOnSequence(const std::string& response);
  void OnCompleteOnSequence(GenerateResult generate_result);

  // The Java counterpart of this object.
  base::android::ScopedJavaGlobalRef<jobject> java_session_;

  // The responder to use for the current `Generate` call. Only storing one
  // responder is fine because `Generate` is only called until the previous one
  // completes.
  mojo::Remote<on_device_model::mojom::StreamingResponder> responder_;
  // The accumulated context of the current session.
  std::vector<ml::InputPiece> context_input_pieces_;

  // The feature for which this session was created.
  const optimization_guide::proto::ModelExecutionFeature feature_;

  // The params used to create this session.
  on_device_model::mojom::SessionParamsPtr params_;

  SEQUENCE_CHECKER(sequence_checker_);
  SequenceCheckerHelper sequence_checker_helper_;

  // The weak pointer created on the main sequence.
  base::WeakPtr<BackendSessionImplAndroid> weak_ptr_;
  base::WeakPtrFactory<BackendSessionImplAndroid> weak_factory_{this};
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_SESSION_IMPL_ANDROID_H_
