// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_RUNTIME_DELEGATE_H_
#define REMOTING_CLIENT_JNI_JNI_RUNTIME_DELEGATE_H_

#include <jni.h>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/telemetry_log_writer.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/chromoting_session.h"
#include "remoting/protocol/connection_to_host.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace remoting {

class JniOAuthTokenGetter;

// JniRuntimeDelegate is a singleton that hooks into delegate role for
// the ChromotingClientRuntime object. This class handles Android specific
// integrations for the runtime. Proxies outgoing JNI calls from
// ChromotingClientRuntime to Java. All its methods should be invoked
// exclusively from the UI thread unless otherwise noted.
class JniRuntimeDelegate : public ChromotingClientRuntime::Delegate {
 public:
  // This class is instantiated at process initialization and persists until
  // we close. Its components are reused across |JniRuntimeDelegate|s.
  static JniRuntimeDelegate* GetInstance();

  // remoting::ChromotingClientRuntime::Delegate overrides.
  void RuntimeWillShutdown() override;
  void RuntimeDidShutdown() override;
  base::WeakPtr<OAuthTokenGetter> oauth_token_getter() override;

 private:
  JniRuntimeDelegate();

  // Forces a DisconnectFromHost() in case there is any active or failed
  // connection, then proceeds to tear down the Chromium dependencies on which
  // all sessions depended. Because destruction only occurs at application exit
  // after all connections have terminated, it is safe to make unretained
  // cross-thread calls on the class.
  ~JniRuntimeDelegate() override;

  // Detaches JVM from the current thread, then signals. Doesn't own |waiter|.
  void DetachFromVmAndSignal(base::WaitableEvent* waiter);

  ChromotingClientRuntime* runtime_;
  std::unique_ptr<JniOAuthTokenGetter> token_getter_;

  friend struct base::DefaultSingletonTraits<JniRuntimeDelegate>;

  DISALLOW_COPY_AND_ASSIGN(JniRuntimeDelegate);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_RUNTIME_DELEGATE_H_
