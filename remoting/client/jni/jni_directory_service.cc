// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_directory_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/android/jni_headers/DirectoryService_jni.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"

#include "base/logging.h"

namespace remoting {

namespace {

JniDirectoryService::RequestError MapError(grpc::StatusCode status_code) {
  switch (status_code) {
    case grpc::UNAVAILABLE:
      return JniDirectoryService::RequestError::SERVICE_UNAVAILABLE;
      break;
    case grpc::PERMISSION_DENIED:
    case grpc::UNAUTHENTICATED:
      return JniDirectoryService::RequestError::AUTH_FAILED;
      break;
    default:
      return JniDirectoryService::RequestError::UNKNOWN;
      break;
  }
}

}  // namespace

JniDirectoryService::JniDirectoryService()
    : grpc_executor_(&token_getter_),
      stub_(apis::v1::RemotingDirectoryService::NewStub(
          CreateSslChannelForEndpoint(
              ServiceUrls::GetInstance()->remoting_server_endpoint()))),
      sequence_(base::SequencedTaskRunnerHandle::Get()) {}

JniDirectoryService::~JniDirectoryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void JniDirectoryService::RetrieveHostList(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  grpc_executor_.ExecuteRpc(CreateGrpcAsyncUnaryRequest(
      base::BindOnce(
          &apis::v1::RemotingDirectoryService::Stub::AsyncGetHostList,
          base::Unretained(stub_.get())),
      apis::v1::GetHostListRequest(),
      base::BindOnce(&JniDirectoryService::OnHostListRetrieved,
                     base::Unretained(this),
                     base::android::ScopedJavaGlobalRef<jobject>(callback))));
}

void JniDirectoryService::DeleteHost(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& host_id,
    const base::android::JavaParamRef<jobject>& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  apis::v1::DeleteHostRequest request;
  request.set_host_id(base::android::ConvertJavaStringToUTF8(env, host_id));
  grpc_executor_.ExecuteRpc(CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&apis::v1::RemotingDirectoryService::Stub::AsyncDeleteHost,
                     base::Unretained(stub_.get())),
      request,
      base::BindOnce(&JniDirectoryService::OnHostDeleted,
                     base::Unretained(this),
                     base::android::ScopedJavaGlobalRef<jobject>(callback))));
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
    const grpc::Status& status,
    const apis::v1::GetHostListResponse& response) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (status.ok()) {
    Java_DirectoryService_onHostListRetrieved(
        env, callback,
        base::android::ToJavaByteArray(env, response.SerializeAsString()));
  } else {
    LOG(ERROR) << "Retrieving host list failed: " << status.error_message();
    Java_DirectoryService_onError(
        env, callback, static_cast<jint>(MapError(status.error_code())));
  }
}

void JniDirectoryService::OnHostDeleted(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const grpc::Status& status,
    const apis::v1::DeleteHostResponse& response) {
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
