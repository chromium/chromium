// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_AUTH_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_REMOTE_AUTH_NATIVE_MESSAGING_HOST_H_

#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace remoting {

// Native messaging host for handling remote authentication requests and sending
// them to the remoting host process via mojo.
class RemoteAuthNativeMessagingHost final
    : public extensions::NativeMessageHost {
 public:
  explicit RemoteAuthNativeMessagingHost(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~RemoteAuthNativeMessagingHost() override;

  void OnMessage(const std::string& message) override;
  void Start(extensions::NativeMessageHost::Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

  RemoteAuthNativeMessagingHost(const RemoteAuthNativeMessagingHost&) = delete;
  RemoteAuthNativeMessagingHost& operator=(
      const RemoteAuthNativeMessagingHost&) = delete;

 private:
  void ProcessHello(base::Value response);

  void SendMessageToClient(base::Value message);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  extensions::NativeMessageHost::Client* client_ = nullptr;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_AUTH_NATIVE_MESSAGING_HOST_H_
