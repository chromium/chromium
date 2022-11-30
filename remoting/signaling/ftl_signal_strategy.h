// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_FTL_SIGNAL_STRATEGY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "remoting/signaling/signal_strategy.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class FtlDeviceIdProvider;
class MessagingClient;
class RegistrationManager;
class SignalingTracker;
class OAuthTokenGetter;

// FtlSignalStrategy implements SignalStrategy using the FTL messaging service.
// This class can be created on a different sequence from the one it is used
// (when Connect() is called).
class FtlSignalStrategy : public SignalStrategy {
 public:
  // We take unique_ptr<OAuthTokenGetter> here so that we still have a chance to
  // send out pending requests after the instance is deleted.
  // |signaling_tracker| is nullable; if it's non-null, it must outlive |this|.
  FtlSignalStrategy(
      std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<FtlDeviceIdProvider> device_id_provider,
      SignalingTracker* signaling_tracker = nullptr);

  FtlSignalStrategy(const FtlSignalStrategy&) = delete;
  FtlSignalStrategy& operator=(const FtlSignalStrategy&) = delete;

  // Note that pending outgoing messages will be silently dropped when the
  // signal strategy is being deleted. If you want to send last minute messages,
  // consider calling Disconnect() then posting a delayed task to delete the
  // strategy.
  ~FtlSignalStrategy() override;

  // SignalStrategy interface.
  void Connect() override;
  void Disconnect() override;
  State GetState() const override;
  Error GetError() const override;
  const SignalingAddress& GetLocalAddress() const override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;
  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza) override;
  bool SendMessage(const SignalingAddress& destination_address,
                   const ftl::ChromotingMessage& message) override;
  std::string GetNextId() override;
  bool IsSignInError() const override;

 private:
  friend class FtlSignalStrategyTest;

  FtlSignalStrategy(std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
                    std::unique_ptr<RegistrationManager> registration_manager,
                    std::unique_ptr<MessagingClient> messaging_client);

  void CreateCore(std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
                  std::unique_ptr<RegistrationManager> registration_manager,
                  std::unique_ptr<MessagingClient> messaging_client);

  // This ensures that even if a Listener deletes the current instance during
  // OnSignalStrategyIncomingStanza(), we can delete |core_| asynchronously.
  class Core;

  std::unique_ptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_SIGNAL_STRATEGY_H_
