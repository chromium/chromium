// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/credentials_type.h"

namespace remoting::protocol {

class FakeChannelAuthenticator : public ChannelAuthenticator {
 public:
  FakeChannelAuthenticator(bool accept, bool async);

  FakeChannelAuthenticator(const FakeChannelAuthenticator&) = delete;
  FakeChannelAuthenticator& operator=(const FakeChannelAuthenticator&) = delete;

  ~FakeChannelAuthenticator() override;

  // ChannelAuthenticator interface.
  void SecureAndAuthenticate(std::unique_ptr<P2PStreamSocket> socket,
                             DoneCallback done_callback) override;

 private:
  void OnAuthBytesWritten(int result);
  void OnAuthBytesRead(int result);

  void CallDoneCallback();

  const int result_;
  const bool async_;

  std::unique_ptr<P2PStreamSocket> socket_;
  DoneCallback done_callback_;

  bool did_read_bytes_ = false;
  bool did_write_bytes_ = false;

  base::WeakPtrFactory<FakeChannelAuthenticator> weak_factory_{this};
};

class FakeAuthenticator : public Authenticator {
 public:
  enum Type {
    HOST,
    CLIENT,
  };

  enum Action { ACCEPT, REJECT, REJECT_CHANNEL };

  struct Config {
    Config();
    Config(Action action);
    Config(int round_trips, Action action, bool async);

    int round_trips = 1;
    Action action = Action::ACCEPT;
    bool async = true;
    raw_ptr<base::RepeatingClosureList> reject_after_accepted;
    CredentialsType credentials_type = CredentialsType::SHARED_SECRET;
  };

  FakeAuthenticator(Type type,
                    Config config,
                    const std::string& local_id,
                    const std::string& remote_id);

  // Special constructor for authenticators in ACCEPTED or REJECTED state that
  // don't exchange any messages.
  FakeAuthenticator(Action action);

  FakeAuthenticator(const FakeAuthenticator&) = delete;
  FakeAuthenticator& operator=(const FakeAuthenticator&) = delete;

  ~FakeAuthenticator() override;

  // Set the number of messages that the authenticator needs to process before
  // started() returns true.  Default to 0.
  void set_messages_till_started(int messages);

  // Sets auth key to be returned by GetAuthKey(). Must be called when
  // |round_trips| is set to 0.
  void set_auth_key(const std::string& auth_key) { auth_key_ = auth_key; }

  // When pause_message_index is set the authenticator will pause in
  // PROCESSING_MESSAGE state after that message, until
  // TakeResumeClosure().Run() is called.
  void set_pause_message_index(int pause_message_index) {
    pause_message_index_ = pause_message_index;
  }
  void Resume();

  // Authenticator interface.
  CredentialsType credentials_type() const override;
  const Authenticator& implementing_authenticator() const override;
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  const SessionPolicies* GetSessionPolicies() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

 protected:
  void SubscribeRejectedAfterAcceptedIfNecessary();

  const Type type_;
  Config config_;
  const std::string local_id_;
  const std::string remote_id_;

  // Total number of messages that have been processed.
  int messages_ = 0;
  // Number of messages that the authenticator needs to process before started()
  // returns true.  Default to 0.
  int messages_till_started_ = 0;

  int pause_message_index_ = -1;
  base::OnceClosure resume_closure_;

  std::string auth_key_;
  base::CallbackListSubscription reject_after_accepted_subscription_;
};

class FakeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  FakeHostAuthenticatorFactory(int messages_till_start,
                               FakeAuthenticator::Config config);

  FakeHostAuthenticatorFactory(const FakeHostAuthenticatorFactory&) = delete;
  FakeHostAuthenticatorFactory& operator=(const FakeHostAuthenticatorFactory&) =
      delete;

  ~FakeHostAuthenticatorFactory() override;

  // AuthenticatorFactory interface.
  std::unique_ptr<Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid) override;

 private:
  const int messages_till_started_;
  const FakeAuthenticator::Config config_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_
