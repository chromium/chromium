// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/android/embedded_test_server_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/test/test_support_android.h"
#include "base/trace_event/trace_event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/android/net_test_support_provider_jni/EmbeddedTestServerImpl_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace net::test_server {

EmbeddedTestServerAndroid::ConnectionListener::ConnectionListener(
    EmbeddedTestServerAndroid* test_server_android)
    : test_server_android_(test_server_android) {}

EmbeddedTestServerAndroid::ConnectionListener::~ConnectionListener() = default;

std::unique_ptr<StreamSocket>
EmbeddedTestServerAndroid::ConnectionListener::AcceptedSocket(
    std::unique_ptr<StreamSocket> socket) {
  test_server_android_->AcceptedSocket(static_cast<const void*>(socket.get()));
  return socket;
}

void EmbeddedTestServerAndroid::ConnectionListener::ReadFromSocket(
    const StreamSocket& socket,
    int rv) {
  test_server_android_->ReadFromSocket(static_cast<const void*>(&socket));
}

EmbeddedTestServerAndroid::EmbeddedTestServerAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& jobj,
    jboolean jhttps)
    : weak_java_server_(env, jobj),
      test_server_(jhttps ? EmbeddedTestServer::TYPE_HTTPS
                          : EmbeddedTestServer::TYPE_HTTP),
      connection_listener_(this) {
  test_server_.SetConnectionListener(&connection_listener_);
  Java_EmbeddedTestServerImpl_setNativePtr(env, jobj,
                                           reinterpret_cast<intptr_t>(this));

  // Register the request monitor to capture request headers.
  test_server_.RegisterRequestMonitor(
      base::BindRepeating(&EmbeddedTestServerAndroid::MonitorResourceRequest,
                          base::Unretained(this)));
}

EmbeddedTestServerAndroid::~EmbeddedTestServerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_EmbeddedTestServerImpl_clearNativePtr(env, weak_java_server_.get(env));
}

jboolean EmbeddedTestServerAndroid::Start(JNIEnv* env, jint port) {
  return test_server_.Start(static_cast<int>(port));
}

ScopedJavaLocalRef<jstring> EmbeddedTestServerAndroid::GetRootCertPemPath(
    JNIEnv* env) const {
  return base::android::ConvertUTF8ToJavaString(
      env, test_server_.GetRootCertPemPath().value());
}

jboolean EmbeddedTestServerAndroid::ShutdownAndWaitUntilComplete(JNIEnv* env) {
  return test_server_.ShutdownAndWaitUntilComplete();
}

ScopedJavaLocalRef<jstring> EmbeddedTestServerAndroid::GetURL(
    JNIEnv* env,
    const JavaRef<jstring>& jrelative_url) const {
  const GURL gurl(test_server_.GetURL(
      base::android::ConvertJavaStringToUTF8(env, jrelative_url)));
  return base::android::ConvertUTF8ToJavaString(env, gurl.spec());
}

ScopedJavaLocalRef<jstring> EmbeddedTestServerAndroid::GetURLWithHostName(
    JNIEnv* env,
    const JavaRef<jstring>& jhostname,
    const JavaRef<jstring>& jrelative_url) const {
  const GURL gurl(test_server_.GetURL(
      base::android::ConvertJavaStringToUTF8(env, jhostname),
      base::android::ConvertJavaStringToUTF8(env, jrelative_url)));
  return base::android::ConvertUTF8ToJavaString(env, gurl.spec());
}

std::vector<std::string> EmbeddedTestServerAndroid::GetRequestHeadersForUrl(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jrelative_url) {
  base::AutoLock auto_lock(lock_);
  std::string path = base::android::ConvertJavaStringToUTF8(env, jrelative_url);
  CHECK(requests_by_path_.contains(path)) << path;

  // Copy headers from HttpRequest::HeaderMap to std::vector for passing them to
  // Java. For the required SDK version in Cronet, std::map is not available
  // (see the comment in IEmbeddedTestServerImpl.aidl for details). The vector
  // alternates between header names (even indices) and their corresponding
  // values (odd indices).
  std::vector<std::string> headers;
  for (auto [key, value] : requests_by_path_[path].headers) {
    headers.push_back(key);
    headers.push_back(value);
  }
  return headers;
}

int EmbeddedTestServerAndroid::GetRequestCountForUrl(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jrelative_url) {
  base::AutoLock auto_lock(lock_);
  std::string path = base::android::ConvertJavaStringToUTF8(env, jrelative_url);
  auto it = requests_by_path_.find(path);
  if (it == requests_by_path_.end()) {
    return 0;
  }
  return it->second.count;
}

void EmbeddedTestServerAndroid::AddDefaultHandlers(
    JNIEnv* env,
    const JavaRef<jstring>& jdirectory_path) {
  const base::FilePath directory(
      base::android::ConvertJavaStringToUTF8(env, jdirectory_path));
  test_server_.AddDefaultHandlers(directory);
}

void EmbeddedTestServerAndroid::SetSSLConfig(JNIEnv* jenv,
                                             jint jserver_certificate) {
  test_server_.SetSSLConfig(
      static_cast<EmbeddedTestServer::ServerCertificate>(jserver_certificate));
}

typedef std::unique_ptr<HttpResponse> (*HandleRequestPtr)(
    const HttpRequest& request);

void EmbeddedTestServerAndroid::RegisterRequestHandler(JNIEnv* env,
                                                       jlong handler) {
  HandleRequestPtr handler_ptr = reinterpret_cast<HandleRequestPtr>(handler);
  test_server_.RegisterRequestHandler(base::BindRepeating(handler_ptr));
}

void EmbeddedTestServerAndroid::ServeFilesFromDirectory(
    JNIEnv* env,
    const JavaRef<jstring>& jdirectory_path) {
  const base::FilePath directory(
      base::android::ConvertJavaStringToUTF8(env, jdirectory_path));
  test_server_.ServeFilesFromDirectory(directory);
}

void EmbeddedTestServerAndroid::AcceptedSocket(const void* socket_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_EmbeddedTestServerImpl_acceptedSocket(
      env, weak_java_server_.get(env), reinterpret_cast<intptr_t>(socket_id));
}

void EmbeddedTestServerAndroid::ReadFromSocket(const void* socket_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_EmbeddedTestServerImpl_readFromSocket(
      env, weak_java_server_.get(env), reinterpret_cast<intptr_t>(socket_id));
}

void EmbeddedTestServerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

static void JNI_EmbeddedTestServerImpl_Init(
    JNIEnv* env,
    const JavaRef<jobject>& jobj,
    const JavaRef<jstring>& jtest_data_dir,
    jboolean jhttps) {
  TRACE_EVENT0("native", "EmbeddedTestServerAndroid::Init");
  base::FilePath test_data_dir(
      base::android::ConvertJavaStringToUTF8(env, jtest_data_dir));
  base::InitAndroidTestPaths(test_data_dir);

  // Bare new does not leak here because the instance deletes itself when it
  // receives a Destroy() call its Java counterpart. The Java counterpart owns
  // the instance created here.
  new EmbeddedTestServerAndroid(env, jobj, jhttps);
}

void EmbeddedTestServerAndroid::MonitorResourceRequest(
    const net::test_server::HttpRequest& request) {
  base::AutoLock auto_lock(lock_);

  // Currently, when multiple requests are sent to the same URL, only the first
  // request can record the headers.
  std::string path = request.GetURL().PathForRequest();
  auto it = requests_by_path_.find(path);
  if (it != requests_by_path_.end()) {
    it->second.count++;
    return;
  }

  RequestInfoByPath info;
  info.headers = request.headers;
  info.count++;
  requests_by_path_.emplace(path, std::move(info));
}

EmbeddedTestServerAndroid::RequestInfoByPath::RequestInfoByPath() = default;
EmbeddedTestServerAndroid::RequestInfoByPath::~RequestInfoByPath() = default;

EmbeddedTestServerAndroid::RequestInfoByPath::RequestInfoByPath(
    EmbeddedTestServerAndroid::RequestInfoByPath&& other) = default;
EmbeddedTestServerAndroid::RequestInfoByPath&
EmbeddedTestServerAndroid::RequestInfoByPath::operator=(
    EmbeddedTestServerAndroid::RequestInfoByPath&& other) = default;
EmbeddedTestServerAndroid::RequestInfoByPath::RequestInfoByPath(
    const EmbeddedTestServerAndroid::RequestInfoByPath& other) = default;
EmbeddedTestServerAndroid::RequestInfoByPath&
EmbeddedTestServerAndroid::RequestInfoByPath::operator=(
    const EmbeddedTestServerAndroid::RequestInfoByPath& other) = default;

}  // namespace net::test_server

DEFINE_JNI(EmbeddedTestServerImpl)
