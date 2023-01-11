// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/secure_channel_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace remoting::protocol {

SecureChannelFactory::SecureChannelFactory(
    StreamChannelFactory* channel_factory,
    Authenticator* authenticator)
    : channel_factory_(channel_factory), authenticator_(authenticator) {
  DCHECK_EQ(authenticator_->state(), Authenticator::ACCEPTED);
}

SecureChannelFactory::~SecureChannelFactory() {
  // CancelChannelCreation() is expected to be called before destruction.
  DCHECK(channel_authenticators_.empty());
}

void SecureChannelFactory::CreateChannel(const std::string& name,
                                         ChannelCreatedCallback callback) {
  DCHECK(!callback.is_null());
  channel_factory_->CreateChannel(
      name, base::BindOnce(&SecureChannelFactory::OnBaseChannelCreated,
                           base::Unretained(this), name, std::move(callback)));
}

void SecureChannelFactory::CancelChannelCreation(const std::string& name) {
  auto it = channel_authenticators_.find(name);
  if (it == channel_authenticators_.end()) {
    channel_factory_->CancelChannelCreation(name);
  } else {
    delete it->second;
    channel_authenticators_.erase(it);
  }
}

void SecureChannelFactory::OnBaseChannelCreated(
    const std::string& name,
    ChannelCreatedCallback callback,
    std::unique_ptr<P2PStreamSocket> socket) {
  if (!socket) {
    std::move(callback).Run(nullptr);
    return;
  }

  ChannelAuthenticator* channel_authenticator =
      authenticator_->CreateChannelAuthenticator().release();
  channel_authenticators_[name] = channel_authenticator;
  channel_authenticator->SecureAndAuthenticate(
      std::move(socket),
      base::BindOnce(&SecureChannelFactory::OnSecureChannelCreated,
                     base::Unretained(this), name, std::move(callback)));
}

void SecureChannelFactory::OnSecureChannelCreated(
    const std::string& name,
    ChannelCreatedCallback callback,
    int error,
    std::unique_ptr<P2PStreamSocket> socket) {
  DCHECK((socket && error == net::OK) || (!socket && error != net::OK));

  auto it = channel_authenticators_.find(name);
  DCHECK(it != channel_authenticators_.end());
  delete it->second;
  channel_authenticators_.erase(it);

  std::move(callback).Run(std::move(socket));
}

}  // namespace remoting::protocol
