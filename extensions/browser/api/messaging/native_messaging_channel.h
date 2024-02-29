// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/values.h"

namespace extensions {

// An interface to receive and send messages between a native component and
// chrome.
class NativeMessagingChannel {
 public:
  // Callback interface for the channel. EventHandler must outlive
  // NativeMessagingChannel.
  class EventHandler {
   public:
    // Called when a message is received from the other endpoint.
    virtual void OnMessage(const base::Value& message) = 0;

    // Called when the channel is disconnected.
    // EventHandler is guaranteed not to be called after OnDisconnect().
    virtual void OnDisconnect() = 0;

    virtual ~EventHandler() {}
  };

  virtual ~NativeMessagingChannel() {}

  // Starts reading and processing messages.
  virtual void Start(EventHandler* event_handler) = 0;

  // Sends a message to the other endpoint.
  virtual void SendMessage(std::optional<base::ValueView> message) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
