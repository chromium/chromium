// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_DIRECTORY_SERVICE_H_
#define REMOTING_CLIENT_JNI_JNI_DIRECTORY_SERVICE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "remoting/base/directory_service_client.h"

namespace remoting {

class ProtobufHttpStatus;

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
  void OnHostListRetrieved(
      base::android::ScopedJavaGlobalRef<jobject> callback,
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::GetHostListResponse> response);
  void OnHostDeleted(base::android::ScopedJavaGlobalRef<jobject> callback,
                     const ProtobufHttpStatus& status,
                     std::unique_ptr<apis::v1::DeleteHostResponse> response);

  base::SequenceBound<DirectoryServiceClient> client_;
  scoped_refptr<base::SequencedTaskRunner> sequence_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<JniDirectoryService> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_DIRECTORY_SERVICE_H_
