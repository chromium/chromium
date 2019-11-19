// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_OAUTH_TOKEN_GETTER_
#define REMOTING_CLIENT_JNI_JNI_OAUTH_TOKEN_GETTER_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

// The OAuthTokenGetter implementation on Android using JNI. Please also read
// documentations in JniOAuthTokenGetter.java.
// This class must be used and destroyed on the same thread after it is created.
class JniOAuthTokenGetter : public OAuthTokenGetter {
 public:
  // This is for generating the Java enum counterpart. Please keep this in sync
  // with OAuthTokenGetter::Status.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chromoting.jni
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: OAuthTokenStatus
  enum JniStatus {
    JNI_STATUS_SUCCESS,
    JNI_STATUS_NETWORK_ERROR,
    JNI_STATUS_AUTH_ERROR,
  };

  JniOAuthTokenGetter();
  ~JniOAuthTokenGetter() override;

  // OAuthTokenGetter overrides.
  void CallWithToken(TokenCallback on_access_token) override;
  void InvalidateCache() override;

  base::WeakPtr<JniOAuthTokenGetter> GetWeakPtr();

 private:
  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<JniOAuthTokenGetter> weak_ptr_;
  base::WeakPtrFactory<JniOAuthTokenGetter> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(JniOAuthTokenGetter);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_OAUTH_TOKEN_GETTER_
