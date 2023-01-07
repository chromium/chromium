// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_CHANNEL_H_
#define HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_CHANNEL_H_

#include "base/containers/span.h"
#include "headless/public/headless_export.h"

namespace headless {

// An interface for sending messages to DevTools.
class HEADLESS_EXPORT HeadlessDevToolsChannel {
 public:
  // An interface for receiving messages from DevTools.
  class Client {
   public:
    virtual ~Client() {}
    // Receives an incoming protocol message from DevTools.
    virtual void ReceiveProtocolMessage(base::span<const uint8_t> message) = 0;
    // Notifies about channel being closed from the DevTools side.
    virtual void ChannelClosed() = 0;
  };

  virtual ~HeadlessDevToolsChannel() {}
  // Sets a peer client which receives the messages from DevTools.
  // |client| must outline this channel. Can be switched on and off
  // multiple times.
  virtual void SetClient(Client* client) = 0;
  // Sends an outgoing protocol message to DevTools.
  virtual void SendProtocolMessage(base::span<const uint8_t> message) = 0;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_CHANNEL_H_
