// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_CORP_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_CORP_SIGNAL_STRATEGY_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/signaling/signal_strategy.h"

namespace net {
class ClientCertStore;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class MessagingClient;
class RsaKeyPair;

// CorpSignalStrategy implements SignalStrategy using the Corp messaging service
class CorpSignalStrategy : public SignalStrategy {
 public:
  using CreateClientCertStoreCallback =
      base::RepeatingCallback<std::unique_ptr<net::ClientCertStore>()>;

  CorpSignalStrategy(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      CreateClientCertStoreCallback client_cert_store_callback,
      const std::string& username,
      scoped_refptr<RsaKeyPair> key_pair);

  CorpSignalStrategy(const CorpSignalStrategy&) = delete;
  CorpSignalStrategy& operator=(const CorpSignalStrategy&) = delete;

  ~CorpSignalStrategy() override;

  // SignalStrategy interface.
  void Connect() override;
  void Disconnect() override;
  State GetState() const override;
  Error GetError() const override;
  const SignalingAddress& GetLocalAddress() const override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;
  bool SendMessage(const SignalingAddress& destination_address,
                   SignalingMessage&& message) override;
  std::string GetNextId() override;
  bool IsSignInError() const override;

 private:
  // CorpSignalStrategyTest uses the private c'tor w/ a fake messaging client.
  friend class CorpSignalStrategyTest;
  CorpSignalStrategy(std::unique_ptr<MessagingClient> messaging_client,
                     const SignalingAddress& local_address);

  class Core;
  std::unique_ptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_CORP_SIGNAL_STRATEGY_H_
