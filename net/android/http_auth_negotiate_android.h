// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_HTTP_AUTH_NEGOTIATE_ANDROID_H_
#define NET_ANDROID_HTTP_AUTH_NEGOTIATE_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_mechanism.h"

namespace base {
class TaskRunner;
}

namespace net {

class HttpAuthChallengeTokenizer;
class HttpAuthPreferences;

namespace android {

// This class provides a threadsafe wrapper for SetResult, which is called from
// Java. A new instance of this class is needed for each call, and the instance
// destroys itself when the callback is received. It is written to allow
// setResult to be called on any thread, but in practice they will be called
// on the application's main thread.
//
// We cannot use a Callback object here, because there is no way of invoking the
// Run method from Java.
class NET_EXPORT_PRIVATE JavaNegotiateResultWrapper {
 public:
  scoped_refptr<base::TaskRunner> callback_task_runner_;
  base::OnceCallback<void(int, const std::string&)> thread_safe_callback_;

  JavaNegotiateResultWrapper(
      const scoped_refptr<base::TaskRunner>& callback_task_runner,
      base::OnceCallback<void(int, const std::string&)> thread_safe_callback);

  void SetResult(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 int result,
                 const base::android::JavaParamRef<jstring>& token);

 private:
  // Class is only allowed to delete itself, nobody else is allowed to delete.
  ~JavaNegotiateResultWrapper();
};

// Class providing Negotiate (SPNEGO/Kerberos) authentication support on
// Android. The actual authentication is done through an Android authenticator
// provided by third parties who want Kerberos support. This class simply
// provides a bridge to the Java code, and hence to the service. See
// https://drive.google.com/open?id=1G7WAaYEKMzj16PTHT_cIYuKXJG6bBcrQ7QQBQ6ihOcQ&authuser=1
// for the full details.
class NET_EXPORT_PRIVATE HttpAuthNegotiateAndroid : public HttpAuthMechanism {
 public:
  // Creates an object for one negotiation session. |prefs| are the
  // authentication preferences. In particular they include the Android account
  // type, which is used to connect to the correct Android Authenticator.
  explicit HttpAuthNegotiateAndroid(const HttpAuthPreferences* prefs);
  HttpAuthNegotiateAndroid(const HttpAuthNegotiateAndroid&) = delete;
  HttpAuthNegotiateAndroid& operator=(const HttpAuthNegotiateAndroid&) = delete;
  ~HttpAuthNegotiateAndroid() override;

  // HttpAuthMechanism implementation:
  bool Init(const NetLogWithSource& net_log) override;
  bool NeedsIdentity() const override;
  bool AllowsExplicitCredentials() const override;
  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuthChallengeTokenizer* tok) override;
  int GenerateAuthToken(const AuthCredentials* credentials,
                        const std::string& spn,
                        const std::string& channel_bindings,
                        std::string* auth_token,
                        const NetLogWithSource& net_log,
                        CompletionOnceCallback callback) override;
  void SetDelegation(HttpAuth::DelegationType delegation_type) override;

  // Unlike the platform agnostic GenerateAuthToken(), the Android specific
  // version doesn't require a NetLogWithSource. The call is made across service
  // boundaries, so currently the goings-on within the GenerateAuthToken()
  // handler is outside the scope of the NetLog.
  int GenerateAuthTokenAndroid(const AuthCredentials* credentials,
                               const std::string& spn,
                               const std::string& channel_bindings,
                               std::string* auth_token,
                               CompletionOnceCallback callback);

  bool can_delegate() const { return can_delegate_; }
  void set_can_delegate(bool can_delegate) { can_delegate_ = can_delegate; }

  const std::string& server_auth_token() const { return server_auth_token_; }
  void set_server_auth_token(const std::string& server_auth_token) {
    server_auth_token_ = server_auth_token;
  }

  std::string GetAuthAndroidNegotiateAccountType() const;

 private:
  void SetResultInternal(int result, const std::string& token);

  raw_ptr<const HttpAuthPreferences> prefs_ = nullptr;
  bool can_delegate_ = false;
  bool first_challenge_ = true;
  std::string server_auth_token_;
  raw_ptr<std::string> auth_token_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> java_authenticator_;
  net::CompletionOnceCallback completion_callback_;

  base::WeakPtrFactory<HttpAuthNegotiateAndroid> weak_factory_{this};
};

}  // namespace android
}  // namespace net

#endif  // NET_ANDROID_HTTP_AUTH_NEGOTIATE_ANDROID_H_
