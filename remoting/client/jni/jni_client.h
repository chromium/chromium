// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_CLIENT_H_
#define REMOTING_CLIENT_JNI_JNI_CLIENT_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_session.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/cursor_shape_stub.h"

namespace remoting {

class ChromotingClientRuntime;
class JniGlDisplayHandler;

struct ConnectToHostInfo;

// Houses resources scoped to a session and exposes JNI interface to the
// Java client during a session. All its methods should be invoked exclusively
// from the UI thread unless otherwise noted.
class JniClient : public ChromotingSession::Delegate {
 public:
  JniClient(base::android::ScopedJavaGlobalRef<jobject> java_client);
  ~JniClient() override;

  // Initiates a connection with the specified host. To skip the attempt at
  // pair-based authentication, leave |pairing_id| and |pairing_secret| as
  // empty strings.
  void ConnectToHost(const ConnectToHostInfo& info);

  // Terminates any ongoing connection attempt and cleans up by nullifying
  // |session_|. This is a no-op unless |session| is currently non-null.
  void DisconnectFromHost();

  // Pops up a dialog box asking the user to enter a PIN. Call on UI thread.
  void DisplayAuthenticationPrompt(bool pairing_supported);

  // ChromotingSession::Delegate implementation

  // Notifies Java code of the current connection status. Call on UI thread.
  void OnConnectionState(protocol::ConnectionToHost::State state,
                         protocol::ErrorCode error) override;

  // Saves new pairing credentials to permanent storage. Call on UI thread.
  void CommitPairingCredentials(const std::string& host,
                                const std::string& id,
                                const std::string& secret) override;

  // Notifies the user interface that the user needs to enter a PIN. The current
  // authentication attempt is put on hold until |callback| is invoked.
  void FetchSecret(
      bool pairing_supported,
      const protocol::SecretFetchedCallback& secret_fetched_callback) override;

  // Pops up a third party login page to fetch token required for
  // authentication. Call on UI thread.
  void FetchThirdPartyToken(
      const std::string& token_url,
      const std::string& client_id,
      const std::string& scopes,
      const protocol::ThirdPartyTokenFetchedCallback& callback) override;

  // Pass on the set of negotiated capabilities to the client.
  void SetCapabilities(const std::string& capabilities) override;

  // Passes on the deconstructed ExtensionMessage to the client to handle
  // appropriately.
  void HandleExtensionMessage(const std::string& type,
                              const std::string& message) override;

  // The following methods are exposed to Java via JNI.

  // TODO(yuweih): Pass a class/struct from Java holding all these arguments.
  void Connect(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& caller,
               const base::android::JavaParamRef<jstring>& username,
               const base::android::JavaParamRef<jstring>& auth_token,
               const base::android::JavaParamRef<jstring>& host_jid,
               const base::android::JavaParamRef<jstring>& host_ftl_id,
               const base::android::JavaParamRef<jstring>& host_id,
               const base::android::JavaParamRef<jstring>& host_pubkey,
               const base::android::JavaParamRef<jstring>& pair_id,
               const base::android::JavaParamRef<jstring>& pair_secret,
               const base::android::JavaParamRef<jstring>& capabilities,
               const base::android::JavaParamRef<jstring>& flags,
               const base::android::JavaParamRef<jstring>& host_version,
               const base::android::JavaParamRef<jstring>& host_os,
               const base::android::JavaParamRef<jstring>& host_os_version);

  void Disconnect(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller);

  void AuthenticationResponse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jstring>& pin,
      jboolean createPair,
      const base::android::JavaParamRef<jstring>& deviceName);

  void SendMouseEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller,
                      jint x,
                      jint y,
                      jint whichButton,
                      jboolean buttonDown);

  void SendMouseWheelEvent(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& caller,
                           jint delta_x,
                           jint delta_y);

  jboolean SendKeyEvent(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller,
                        jint scanCode,
                        jint keyCode,
                        jboolean keyDown);

  void SendTextEvent(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& caller,
                     const base::android::JavaParamRef<jstring>& text);

  void SendTouchEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      jint eventType,
      const base::android::JavaParamRef<jobjectArray>& touchEventObjectArray);

  void EnableVideoChannel(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& caller,
                          jboolean enable);

  void OnThirdPartyTokenFetched(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jstring>& token,
      const base::android::JavaParamRef<jstring>& shared_secret);

  void SendExtensionMessage(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& caller,
                            const base::android::JavaParamRef<jstring>& type,
                            const base::android::JavaParamRef<jstring>& data);

  void SendClientResolution(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& caller,
                            jint dips_width,
                            jint dips_height,
                            jfloat scale);

  // Deletes this object.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& caller);

  // Get the weak pointer of the object. Should be used on the UI thread.
  // TODO(yuweih): Instead of exposing a weak pointer in the public API,
  //               consider handling task posting inside the client.
  base::WeakPtr<JniClient> GetWeakPtr();

 private:
  ChromotingClientRuntime* runtime_;

  // Reference to the Java client object.
  base::android::ScopedJavaGlobalRef<jobject> java_client_;

  std::unique_ptr<JniGlDisplayHandler> display_handler_;

  std::string host_id_;

  protocol::SecretFetchedCallback secret_fetched_callback_;
  protocol::ThirdPartyTokenFetchedCallback third_party_token_fetched_callback_;

  // Deleted on Network thread.
  std::unique_ptr<ChromotingSession> session_;

  // Holds pointer for the UI thread.
  base::WeakPtr<JniClient> weak_ptr_;
  base::WeakPtrFactory<JniClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(JniClient);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_CLIENT_H_
