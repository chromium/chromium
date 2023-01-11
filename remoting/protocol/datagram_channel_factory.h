// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DATAGRAM_CHANNEL_FACTORY_H_
#define REMOTING_PROTOCOL_DATAGRAM_CHANNEL_FACTORY_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace remoting::protocol {

class P2PDatagramSocket;

class DatagramChannelFactory {
 public:
  typedef base::OnceCallback<void(std::unique_ptr<P2PDatagramSocket>)>
      ChannelCreatedCallback;

  DatagramChannelFactory() = default;

  DatagramChannelFactory(const DatagramChannelFactory&) = delete;
  DatagramChannelFactory& operator=(const DatagramChannelFactory&) = delete;

  // Creates new channels and calls the |callback| when then new channel is
  // created and connected. The |callback| is called with nullptr if channel
  // setup failed for any reason. Callback may be called synchronously, before
  // the call returns. All channels must be destroyed, and
  // CancelChannelCreation() called for any pending channels, before the factory
  // is destroyed.
  virtual void CreateChannel(const std::string& name,
                             ChannelCreatedCallback callback) = 0;

  // Cancels a pending CreateChannel() operation for the named channel. If the
  // channel creation already completed then canceling it has no effect. When
  // shutting down this method must be called for each channel pending creation.
  virtual void CancelChannelCreation(const std::string& name) = 0;

 protected:
  virtual ~DatagramChannelFactory() = default;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_DATAGRAM_CHANNEL_FACTORY_H_
