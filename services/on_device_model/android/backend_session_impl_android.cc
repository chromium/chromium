// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/backend_session_impl_android.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/on_device_model/android/on_device_model_bridge.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/jni_headers/AiCoreSession_jni.h"

namespace on_device_model {

BackendSessionImplAndroid::BackendSessionImplAndroid()
    : java_session_(OnDeviceModelBridge::CreateSession()) {}

BackendSessionImplAndroid::~BackendSessionImplAndroid() = default;

void BackendSessionImplAndroid::Append(
    on_device_model::mojom::AppendOptionsPtr options,
    mojo::PendingRemote<on_device_model::mojom::ContextClient> client,
    base::OnceClosure on_complete) {
  NOTIMPLEMENTED();
  std::move(on_complete).Run();
}

void BackendSessionImplAndroid::Generate(
    on_device_model::mojom::GenerateOptionsPtr input,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response,
    base::OnceClosure on_complete) {
  responder_.Bind(std::move(response));

  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(crbug.com/425408635): Pass the real input.
  base::android::ScopedJavaLocalRef<jstring> j_input =
      base::android::ConvertUTF8ToJavaString(env, "");
  Java_AiCoreSession_generate(env, java_session_,
                              reinterpret_cast<intptr_t>(this), j_input);
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

void BackendSessionImplAndroid::OnResponse(const std::string& response) {
  auto chunk = on_device_model::mojom::ResponseChunk::New();
  chunk->text = response;
  responder_->OnResponse(std::move(chunk));
}

void BackendSessionImplAndroid::OnComplete() {
  responder_->OnComplete(on_device_model::mojom::ResponseSummary::New());
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
