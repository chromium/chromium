// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SECURE_CHANNEL_FACTORY_H_
#define REMOTING_PROTOCOL_SECURE_CHANNEL_FACTORY_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting::protocol {

class Authenticator;
class ChannelAuthenticator;

// StreamChannelFactory wrapper that authenticates every channel it creates.
// When CreateChannel() is called it first calls the wrapped
// StreamChannelFactory to create a channel and then uses the specified
// Authenticator to secure and authenticate the new channel before returning it
// to the caller.
class SecureChannelFactory : public StreamChannelFactory {
 public:
  // Both parameters must outlive the object.
  SecureChannelFactory(StreamChannelFactory* channel_factory,
                       Authenticator* authenticator);

  SecureChannelFactory(const SecureChannelFactory&) = delete;
  SecureChannelFactory& operator=(const SecureChannelFactory&) = delete;

  ~SecureChannelFactory() override;

  // StreamChannelFactory interface.
  void CreateChannel(const std::string& name,
                     ChannelCreatedCallback callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  typedef std::map<std::string, raw_ptr<ChannelAuthenticator, CtnExperimental>>
      AuthenticatorMap;

  void OnBaseChannelCreated(const std::string& name,
                            ChannelCreatedCallback callback,
                            std::unique_ptr<P2PStreamSocket> socket);

  void OnSecureChannelCreated(const std::string& name,
                              ChannelCreatedCallback callback,
                              int error,
                              std::unique_ptr<P2PStreamSocket> socket);

  raw_ptr<StreamChannelFactory> channel_factory_;
  raw_ptr<Authenticator> authenticator_;

  AuthenticatorMap channel_authenticators_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_SECURE_CHANNEL_FACTORY_H_
