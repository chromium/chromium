// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_session_impl_android.h"

#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/on_device_model/android/on_device_model_bridge.h"
#include "services/on_device_model/ml/chrome_ml_types.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/jni_headers/AiCoreSession_jni.h"
#include "services/on_device_model/android/jni_headers/InputPieceHelper_jni.h"

namespace on_device_model {

BackendSessionImplAndroid::BackendSessionImplAndroid(
    optimization_guide::proto::ModelExecutionFeature feature,
    on_device_model::mojom::SessionParamsPtr params)
    : java_session_(
          OnDeviceModelBridge::CreateSession(feature, std::move(params))) {}

BackendSessionImplAndroid::~BackendSessionImplAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AiCoreSession_onNativeDestroyed(env, java_session_);
}

void BackendSessionImplAndroid::Append(
    on_device_model::mojom::AppendOptionsPtr options,
    mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
    base::OnceClosure on_complete) {
  context_input_pieces_.insert(context_input_pieces_.end(),
                               options->input->pieces.begin(),
                               options->input->pieces.end());
  std::move(on_complete).Run();
}

void BackendSessionImplAndroid::Generate(
    on_device_model::mojom::GenerateOptionsPtr input,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response,
    base::OnceClosure on_complete) {
  CHECK(!responder_.is_bound()) << "Caller should not call Generate() again "
                                   "before OnComplete() is received.";
  responder_.Bind(std::move(response));

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<base::android::ScopedJavaLocalRef<jobject>> java_inputs;
  for (const auto& piece : context_input_pieces_) {
    if (std::holds_alternative<ml::Token>(piece)) {
      java_inputs.push_back(Java_InputPieceHelper_fromToken(
          env, static_cast<int>(std::get<ml::Token>(piece))));
    } else if (std::holds_alternative<std::string>(piece)) {
      java_inputs.push_back(Java_InputPieceHelper_fromText(
          env, base::android::ConvertUTF8ToJavaString(
                   env, std::get<std::string>(piece))));
    } else {
      // TODO(crbug.com/425408635): Support image and audio input.
      NOTREACHED();
    }
  }

  Java_AiCoreSession_generate(
      env, java_session_, reinterpret_cast<intptr_t>(this),
      base::android::ToJavaArrayOfObjects(env, java_inputs));
  std::move(on_complete).Run();
}

void BackendSessionImplAndroid::SizeInTokens(
    on_device_model::mojom::InputPtr input,
    base::OnceCallback<void(uint32_t)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(0);
}

void BackendSessionImplAndroid::Score(
    const std::string& text,
    base::OnceCallback<void(float)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(0.0f);
}

void BackendSessionImplAndroid::GetProbabilitiesBlocking(
    const std::string& input,
    base::OnceCallback<void(const std::vector<float>&)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run({});
}

std::unique_ptr<BackendSession> BackendSessionImplAndroid::Clone() {
  NOTIMPLEMENTED();
  return nullptr;
}

void BackendSessionImplAndroid::AsrStream(
    on_device_model::mojom::AsrStreamOptionsPtr options,
    mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder) {
  NOTIMPLEMENTED();
}

void BackendSessionImplAndroid::AsrAddAudioChunk(
    on_device_model::mojom::AudioDataPtr data) {
  NOTIMPLEMENTED();
}

void BackendSessionImplAndroid::OnResponse(const std::string& response) {
  auto chunk = on_device_model::mojom::ResponseChunk::New();
  chunk->text = response;
  responder_->OnResponse(std::move(chunk));
}

void BackendSessionImplAndroid::OnComplete() {
  responder_->OnComplete(on_device_model::mojom::ResponseSummary::New());
  responder_.reset();
}

void JNI_AiCoreSession_OnComplete(JNIEnv* env, jlong backend_session) {
  reinterpret_cast<BackendSessionImplAndroid*>(backend_session)->OnComplete();
}

void JNI_AiCoreSession_OnResponse(
    JNIEnv* env,
    jlong backend_session,
    const jni_zero::JavaParamRef<jstring>& j_response) {
  reinterpret_cast<BackendSessionImplAndroid*>(backend_session)
      ->OnResponse(base::android::ConvertJavaStringToUTF8(env, j_response));
}

}  // namespace on_device_model
