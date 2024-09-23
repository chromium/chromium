// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VALIDATING_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_VALIDATING_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"

namespace remoting::protocol {

// This authenticator class provides a way to check the validity of a connection
// as it is being established through an asynchronous callback.  The validation
// logic supplied by the caller is run once the underlying authenticator(s) have
// accepted the connection.
class ValidatingAuthenticator : public Authenticator {
 public:
  enum class Result {
    SUCCESS,
    ERROR_INVALID_CREDENTIALS,
    ERROR_INVALID_ACCOUNT,
    ERROR_REJECTED_BY_USER,
    ERROR_TOO_MANY_CONNECTIONS,
    ERROR_UNAUTHORIZED_ACCOUNT
  };

  typedef base::OnceCallback<void(Result validation_result)> ResultCallback;

  typedef base::RepeatingCallback<void(const std::string& remote_jid,
                                       ResultCallback callback)>
      ValidationCallback;

  ValidatingAuthenticator(const std::string& remote_jid,
                          const ValidationCallback& validation_callback,
                          std::unique_ptr<Authenticator> current_authenticator);

  ValidatingAuthenticator(const ValidatingAuthenticator&) = delete;
  ValidatingAuthenticator& operator=(const ValidatingAuthenticator&) = delete;

  ~ValidatingAuthenticator() override;

  // Authenticator interface.
  CredentialsType credentials_type() const override;
  const Authenticator& implementing_authenticator() const override;
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  const std::string& GetAuthKey() const override;
  const SessionPolicies* GetSessionPolicies() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;

 private:
  // Checks |result|.  If the connection was rejected, |state_| and
  // |rejection_reason_| are updated.  |callback| is always run.
  void OnValidateComplete(base::OnceClosure callback, Result result);

  // Updates |state_| to reflect the current underlying authenticator state.
  // |resume_callback| is called after the state is updated.
  void UpdateState(base::OnceClosure resume_callback);

  void NotifyStateChangeAfterAccepted() override;

  // The JID of the remote user.
  std::string remote_jid_;

  // Called for validation of incoming connection requests.
  ValidationCallback validation_callback_;

  // Returns the current state of the authenticator.
  State state_ = Authenticator::WAITING_MESSAGE;

  // Returns the rejection reason. Can be called only when in REJECTED state.
  RejectionReason rejection_reason_ =
      Authenticator::RejectionReason::INVALID_CREDENTIALS;

  std::unique_ptr<Authenticator> current_authenticator_;

  std::unique_ptr<jingle_xmpp::XmlElement> pending_auth_message_;

  base::WeakPtrFactory<ValidatingAuthenticator> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_VALIDATING_AUTHENTICATOR_H_
