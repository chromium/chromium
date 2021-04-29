// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {

// The C++ side of the Java EmbeddedTestServer.
class EmbeddedTestServerAndroid {
 public:
  EmbeddedTestServerAndroid(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            jboolean jhttps);
  ~EmbeddedTestServerAndroid();

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  jboolean Start(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jobj,
                 jint port);

  base::android::ScopedJavaLocalRef<jstring> GetRootCertPemPath(
      JNIEnv* jenv,
      const base::android::JavaParamRef<jobject>& jobj) const;

  jboolean ShutdownAndWaitUntilComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

  base::android::ScopedJavaLocalRef<jstring> GetURL(
      JNIEnv* jenv,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jrelative_url) const;

  base::android::ScopedJavaLocalRef<jstring> GetURLWithHostName(
      JNIEnv* jenv,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jhostname,
      const base::android::JavaParamRef<jstring>& jrelative_url) const;

  void AddDefaultHandlers(
      JNIEnv* jenv,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jdirectory_path);

  void SetSSLConfig(JNIEnv* jenv,
                    const base::android::JavaParamRef<jobject>& jobj,
                    jint jserver_certificate);

  void RegisterRequestHandler(JNIEnv* jenv,
                              const base::android::JavaParamRef<jobject>& jobj,
                              jlong handler);

  void ServeFilesFromDirectory(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jdirectory_path);

 private:
  // Connection listener forwarding notifications to EmbeddedTestServerAndroid.
  class ConnectionListener : public EmbeddedTestServerConnectionListener {
   public:
    ConnectionListener(EmbeddedTestServerAndroid* test_server_android);
    ~ConnectionListener() override;

    std::unique_ptr<StreamSocket> AcceptedSocket(
        std::unique_ptr<StreamSocket> socket) override;
    void ReadFromSocket(const StreamSocket& socket, int rv) override;
    void OnResponseCompletedSuccessfully(
        std::unique_ptr<StreamSocket> socket) override;

   private:
    EmbeddedTestServerAndroid* test_server_android_;
  };

  // Forwards notifications to Java. See EmbeddedTestServerConnectionListener.
  void AcceptedSocket(const void* socket_id);
  void ReadFromSocket(const void* socket_id);

  JavaObjectWeakGlobalRef weak_java_server_;

  EmbeddedTestServer test_server_;
  ConnectionListener connection_listener_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedTestServerAndroid);
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_
