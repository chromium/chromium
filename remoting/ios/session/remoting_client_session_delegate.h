// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_SESSION_REMOTING_CLIENT_SESSION_DELEGATE_H_
#define REMOTING_IOS_SESSION_REMOTING_CLIENT_SESSION_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_session.h"
#include "remoting/protocol/connection_to_host.h"

@class RemotingClient;

namespace remoting {

class ChromotingClientRuntime;

class RemotingClientSessionDelegate : public ChromotingSession::Delegate {
 public:
  RemotingClientSessionDelegate(RemotingClient* client);

  RemotingClientSessionDelegate(const RemotingClientSessionDelegate&) = delete;
  RemotingClientSessionDelegate& operator=(const RemotingClientSessionDelegate&) =
      delete;

  ~RemotingClientSessionDelegate() override;

  // ChromotingSession::Delegate implementation
  void OnConnectionState(protocol::ConnectionToHost::State state,
                         protocol::ErrorCode error) override;
  void CommitPairingCredentials(const std::string& host,
                                const std::string& id,
                                const std::string& secret) override;
  void FetchSecret(
      bool pairing_supported,
      const protocol::SecretFetchedCallback& secret_fetched_callback) override;
  void SetCapabilities(const std::string& capabilities) override;
  void HandleExtensionMessage(const std::string& type,
                              const std::string& message) override;

  base::WeakPtr<RemotingClientSessionDelegate> GetWeakPtr();

 private:
  raw_ptr<ChromotingClientRuntime> runtime_;
  __weak id client_;

  base::WeakPtrFactory<RemotingClientSessionDelegate> weak_factory_;
};

}  // namespace remoting

#endif  // REMOTING_IOS_SESSION_REMOTING_CLIENT_SESSION_DELEGATE_H_
