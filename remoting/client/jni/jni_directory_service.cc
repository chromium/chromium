// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_directory_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/android/jni_headers/DirectoryService_jni.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/task_util.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "base/logging.h"

namespace remoting {

namespace {

JniDirectoryService::RequestError MapError(
    ProtobufHttpStatus::Code status_code) {
  switch (status_code) {
    case ProtobufHttpStatus::Code::UNAVAILABLE:
      return JniDirectoryService::RequestError::SERVICE_UNAVAILABLE;
    case ProtobufHttpStatus::Code::PERMISSION_DENIED:
    case ProtobufHttpStatus::Code::UNAUTHENTICATED:
      return JniDirectoryService::RequestError::AUTH_FAILED;
    default:
      return JniDirectoryService::RequestError::UNKNOWN;
  }
}

}  // namespace

JniDirectoryService::JniDirectoryService()
    : client_(ChromotingClientRuntime::GetInstance()
                  ->CreateDirectoryServiceClient()),
      sequence_(base::SequencedTaskRunner::GetCurrentDefault()) {}

JniDirectoryService::~JniDirectoryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void JniDirectoryService::RetrieveHostList(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PostWithCallback(
      FROM_HERE, &client_, &DirectoryServiceClient::GetHostList,
      base::BindOnce(&JniDirectoryService::OnHostListRetrieved,
                     weak_factory_.GetWeakPtr(),
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

void JniDirectoryService::DeleteHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& host_id,
    const base::android::JavaParamRef<jobject>& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PostWithCallback(
      FROM_HERE, &client_, &DirectoryServiceClient::DeleteHost,
      base::BindOnce(&JniDirectoryService::OnHostDeleted,
                     weak_factory_.GetWeakPtr(),
                     base::android::ScopedJavaGlobalRef<jobject>(callback)),
      base::android::ConvertJavaStringToUTF8(env, host_id));
}

void JniDirectoryService::Destroy(JNIEnv* env) {
  if (sequence_->RunsTasksInCurrentSequence()) {
    delete this;
  } else {
    sequence_->DeleteSoon(FROM_HERE, this);
  }
}

void JniDirectoryService::OnHostListRetrieved(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::GetHostListResponse> response) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (status.ok()) {
    Java_DirectoryService_onHostListRetrieved(
        env, callback,
        base::android::ToJavaByteArray(env, response->SerializeAsString()));
  } else {
    LOG(ERROR) << "Retrieving host list failed: " << status.error_message();
    Java_DirectoryService_onError(
        env, callback, static_cast<jint>(MapError(status.error_code())));
  }
}

void JniDirectoryService::OnHostDeleted(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::DeleteHostResponse> response) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (status.ok()) {
    Java_DirectoryService_onHostDeleted(env, callback);
  } else {
    LOG(ERROR) << "Deleting host failed: " << status.error_message();
    // TODO(rkjnsn): Translate error code from status.
    Java_DirectoryService_onError(
        env, callback, static_cast<jint>(MapError(status.error_code())));
  }
}

static jlong JNI_DirectoryService_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new JniDirectoryService());
}

}  // namespace remoting
