// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net::test_server {

// The C++ side of the Java EmbeddedTestServer.
class EmbeddedTestServerAndroid {
 public:
  EmbeddedTestServerAndroid(JNIEnv* env,
                            const base::android::JavaRef<jobject>& obj,
                            jboolean jhttps);

  EmbeddedTestServerAndroid(const EmbeddedTestServerAndroid&) = delete;
  EmbeddedTestServerAndroid& operator=(const EmbeddedTestServerAndroid&) =
      delete;

  ~EmbeddedTestServerAndroid();

  void Destroy(JNIEnv* env);

  jboolean Start(JNIEnv* env, jint port);

  base::android::ScopedJavaLocalRef<jstring> GetRootCertPemPath(
      JNIEnv* jenv) const;

  jboolean ShutdownAndWaitUntilComplete(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jstring> GetURL(
      JNIEnv* jenv,
      const base::android::JavaRef<jstring>& jrelative_url) const;

  base::android::ScopedJavaLocalRef<jstring> GetURLWithHostName(
      JNIEnv* jenv,
      const base::android::JavaRef<jstring>& jhostname,
      const base::android::JavaRef<jstring>& jrelative_url) const;

  std::vector<std::string> GetRequestHeadersForUrl(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jrelative_url);
  int GetRequestCountForUrl(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jrelative_url);

  void AddDefaultHandlers(
      JNIEnv* jenv,
      const base::android::JavaRef<jstring>& jdirectory_path);

  void SetSSLConfig(JNIEnv* jenv, jint jserver_certificate);

  void RegisterRequestHandler(JNIEnv* jenv, jlong handler);

  void ServeFilesFromDirectory(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jdirectory_path);

 private:
  // Connection listener forwarding notifications to EmbeddedTestServerAndroid.
  class ConnectionListener : public EmbeddedTestServerConnectionListener {
   public:
    explicit ConnectionListener(EmbeddedTestServerAndroid* test_server_android);
    ~ConnectionListener() override;

    std::unique_ptr<StreamSocket> AcceptedSocket(
        std::unique_ptr<StreamSocket> socket) override;
    void ReadFromSocket(const StreamSocket& socket, int rv) override;

   private:
    raw_ptr<EmbeddedTestServerAndroid> test_server_android_;
  };

  // Forwards notifications to Java. See EmbeddedTestServerConnectionListener.
  void AcceptedSocket(const void* socket_id);
  void ReadFromSocket(const void* socket_id);

  void MonitorResourceRequest(const net::test_server::HttpRequest& request);

  JavaObjectWeakGlobalRef weak_java_server_;

  EmbeddedTestServer test_server_;
  ConnectionListener connection_listener_;

  // Headers and counts of requests sent to the server. Keyed by path (not by
  // full URL) because the host part of the requests is translated ("a.test" to
  // "127.0.0.1") before the server handles them.
  // This is accessed from the UI thread and `EmbeddedTestServer::io_thread_`,
  // so it's guarded by the lock.
  struct RequestInfoByPath {
    RequestInfoByPath();
    ~RequestInfoByPath();

    // Movable and copyable.
    RequestInfoByPath(RequestInfoByPath&& other);
    RequestInfoByPath& operator=(RequestInfoByPath&& other);
    RequestInfoByPath(const RequestInfoByPath& other);
    RequestInfoByPath& operator=(const RequestInfoByPath& other);

    // Headers of requests sent for the path.
    net::test_server::HttpRequest::HeaderMap headers;
    // Counts of requests.
    size_t count = 0;
  };
  std::map<std::string, RequestInfoByPath> requests_by_path_ GUARDED_BY(lock_);
  base::Lock lock_;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_
