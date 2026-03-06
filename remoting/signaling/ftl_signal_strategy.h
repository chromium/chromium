// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_FTL_SIGNAL_STRATEGY_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "remoting/signaling/signal_strategy.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

namespace ftl {
class ChromotingMessage;
}  // namespace ftl

class FtlDeviceIdProvider;
class FtlMessagingClient;
class RegistrationManager;
class SignalingTracker;
class OAuthTokenGetter;

// FtlSignalStrategy implements SignalStrategy using the FTL messaging service.
// This class can be created on a different sequence from the one it is used
// (when Connect() is called).
class FtlSignalStrategy : public SignalStrategy {
 public:
  class FtlListener : public base::CheckedObserver {
   public:
    ~FtlListener() override = default;

    // Must return true if the message was handled, false otherwise.
    virtual bool OnIncomingFtlMessage(
        const SignalingAddress& sender_address,
        const ftl::ChromotingMessage& message) = 0;
  };

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
  bool SendMessage(JingleMessage&& message) override;
  bool SendReply(JingleMessageReply&& message) override;

  // Sends an FTL message. Returns false if the message couldn't be sent.
  virtual bool SendFtlMessage(const SignalingAddress& destination_address,
                              ftl::ChromotingMessage&& message);

  virtual void AddFtlListener(FtlListener* listener);
  virtual void RemoveFtlListener(FtlListener* listener);

  std::string GetNextId() override;
  bool IsSignInError() const override;

 protected:
  friend class FtlSignalStrategyTest;

  FtlSignalStrategy();

  FtlSignalStrategy(std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
                    std::unique_ptr<RegistrationManager> registration_manager,
                    std::unique_ptr<FtlMessagingClient> messaging_client);

  void CreateCore(std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
                  std::unique_ptr<RegistrationManager> registration_manager,
                  std::unique_ptr<FtlMessagingClient> messaging_client);

  // This ensures that even if a Listener deletes the current instance during
  // OnSignalingMessage(), we can delete |core_| asynchronously.
  class Core;

  std::unique_ptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_SIGNAL_STRATEGY_H_
