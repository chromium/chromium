// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_SESSION_IMPL_ANDROID_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_SESSION_IMPL_ANDROID_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/backend_session.h"

namespace on_device_model {

// Android implementation of BackendSession. A Java counterpart with the same
// lifetime will be created when this object is created.
class BackendSessionImplAndroid : public BackendSession {
 public:
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

  // Called by Java:
  // Called when the response of `Generate` is received from the AiCoreSession.
  void OnResponse(const std::string& response);
  // Called when the response of `Generate` is completed from the AiCoreSession.
  void OnComplete();

 private:
  // The Java counterpart of this object.
  base::android::ScopedJavaGlobalRef<jobject> java_session_;

  // The responder to use for the current `Generate` call. Only storing one
  // responder is fine because `Generate` is only called until the previous one
  // completes.
  mojo::Remote<on_device_model::mojom::StreamingResponder> responder_;
  // The accumulated context of the current session.
  std::vector<ml::InputPiece> context_input_pieces_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_BACKEND_SESSION_IMPL_ANDROID_H_
