// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_DIRECTORY_SERVICE_H_
#define REMOTING_CLIENT_JNI_JNI_DIRECTORY_SERVICE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/client/jni/jni_oauth_token_getter.h"
#include "remoting/proto/remoting/v1/directory_service.grpc.pb.h"

namespace remoting {

class JniDirectoryService {
 public:
  // TODO(rkjnsn): Update error codes to better align with those returned by the
  // new gRPC API.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromoting.jni
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: DirectoryServiceRequestError
  enum class RequestError : int {
    AUTH_FAILED = 0,
    NETWORK_ERROR = 1,
    SERVICE_UNAVAILABLE = 2,
    UNEXPECTED_RESPONSE = 3,
    UNKNOWN = 4,
  };
  JniDirectoryService();
  ~JniDirectoryService();

  void RetrieveHostList(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& callback);
  void DeleteHost(JNIEnv* env,
                  const base::android::JavaParamRef<jstring>& host_id,
                  const base::android::JavaParamRef<jobject>& callback);

  void Destroy(JNIEnv* env);

 private:
  void OnHostListRetrieved(base::android::ScopedJavaGlobalRef<jobject> callback,
                           const grpc::Status& status,
                           const apis::v1::GetHostListResponse& response);
  void OnHostDeleted(base::android::ScopedJavaGlobalRef<jobject> callback,
                     const grpc::Status& status,
                     const apis::v1::DeleteHostResponse& response);

  JniOAuthTokenGetter token_getter_;
  GrpcAuthenticatedExecutor grpc_executor_;
  std::unique_ptr<apis::v1::RemotingDirectoryService::Stub> stub_;
  scoped_refptr<base::SequencedTaskRunner> sequence_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_DIRECTORY_SERVICE_H_
