// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "jingle/notifier/base/xmpp_connection.h"
#include "jingle/notifier/communicator/connection_settings.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "third_party/libjingle_xmpp/xmpp/xmppengine.h"

namespace jingle_xmpp {
class XmppTaskParentInterface;
}  // namespace jingle_xmpp

namespace notifier {

struct ServerInformation;

// Handles all of the aspects of a single login attempt.  By
// containing this within one class, when another login attempt is
// made, this class can be destroyed to immediately stop the previous
// login attempt.
class SingleLoginAttempt : public XmppConnection::Delegate {
 public:
  // At most one delegate method will be called, depending on the
  // result of the login attempt.  After the delegate method is
  // called, this class won't do anything anymore until it is
  // destroyed, at which point it will disconnect if necessary.
  class Delegate {
   public:
    // Called when the login attempt is successful.
    virtual void OnConnect(
        base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task) = 0;

    // Called when the server responds with a redirect.  A new login
    // attempt should be made to the given redirect server.
    virtual void OnRedirect(const ServerInformation& redirect_server) = 0;

    // Called when a server rejects the client's login credentials.  A
    // new login attempt should be made once the client provides new
    // credentials.
    virtual void OnCredentialsRejected() = 0;

    // Called when no server could be logged into for reasons other
    // than redirection or rejected credentials.  A new login attempt
    // may be created, but it should be done with exponential backoff.
    virtual void OnSettingsExhausted() = 0;

   protected:
    virtual ~Delegate();
  };

  // Does not take ownership of |delegate|, which must not be NULL.
  SingleLoginAttempt(const LoginSettings& login_settings, Delegate* delegate);

  ~SingleLoginAttempt() override;

  // XmppConnection::Delegate implementation.
  void OnConnect(base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> parent) override;
  void OnError(jingle_xmpp::XmppEngine::Error error,
               int error_subcode,
               const jingle_xmpp::XmlElement* stream_error) override;

 private:
  void TryConnect(const ConnectionSettings& new_settings);

  const LoginSettings login_settings_;
  Delegate* const delegate_;
  const ConnectionSettingsList settings_list_;
  ConnectionSettingsList::const_iterator current_settings_;
  std::unique_ptr<XmppConnection> xmpp_connection_;

  DISALLOW_COPY_AND_ASSIGN(SingleLoginAttempt);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_SINGLE_LOGIN_ATTEMPT_H_
