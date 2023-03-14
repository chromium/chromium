// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_PIPE_H_
#define REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_PIPE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/api/messaging/native_messaging_channel.h"

namespace base {
class Value;
}

namespace remoting {

// Connects a extensions::NativeMessageHost to a PipeMessagingChannel.
class NativeMessagingPipe
    : public extensions::NativeMessagingChannel::EventHandler,
      public extensions::NativeMessageHost::Client {
 public:
  NativeMessagingPipe();

  NativeMessagingPipe(const NativeMessagingPipe&) = delete;
  NativeMessagingPipe& operator=(const NativeMessagingPipe&) = delete;

  ~NativeMessagingPipe() override;

  // Starts processing messages from the pipe.
  void Start(std::unique_ptr<extensions::NativeMessageHost> host,
             std::unique_ptr<extensions::NativeMessagingChannel> channel);

  // extensions::NativeMessageHost::Client implementation.
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

  // extensions::NativeMessagingChannel::EventHandler implementation.
  void OnMessage(const base::Value& message) override;
  void OnDisconnect() override;

 private:
  std::unique_ptr<extensions::NativeMessagingChannel> channel_;
  std::unique_ptr<extensions::NativeMessageHost> host_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_PIPE_H_
