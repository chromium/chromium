// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_ELEVATED_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_WIN_ELEVATED_NATIVE_MESSAGING_HOST_H_

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/api/messaging/native_messaging_channel.h"
#include "remoting/host/win/launch_native_messaging_host_process.h"

namespace base {
class Value;
}  // namespace base

namespace remoting {

// Helper class which manages the creation and lifetime of an elevated native
// messaging host process.
class ElevatedNativeMessagingHost
    : public extensions::NativeMessagingChannel::EventHandler {
 public:
  ElevatedNativeMessagingHost(const base::FilePath& binary_path,
                              intptr_t parent_window_handle,
                              bool elevate_process,
                              base::TimeDelta host_timeout,
                              extensions::NativeMessageHost::Client* client);
  ~ElevatedNativeMessagingHost() override;

  // extensions::NativeMessagingChannel::EventHandle implementation.
  void OnMessage(std::unique_ptr<base::Value> message) override;
  void OnDisconnect() override;

  // Create and connect to an elevated host process if necessary.
  // |elevated_channel_| will contain the native messaging channel to the
  // elevated host if the function succeeds.
  ProcessLaunchResult EnsureElevatedHostCreated();

  // Send |message| to the elevated host.
  void SendMessage(std::unique_ptr<base::Value> message);

 private:
  // Disconnect and shut down the elevated host.
  void DisconnectHost();

  // Path to the binary to use for the elevated host process.
  base::FilePath host_binary_path_;

  // Handle of the parent window.
  intptr_t parent_window_handle_;

  // Indicates whether the launched process should be elevated when lauinched.
  // Note: Binaries with uiaccess run at a higher UIPI level than the launching
  // process so they still need to be launched and controlled by this class but
  // do not require traditional elevation to function.
  bool elevate_host_process_;

  // Specifies the amount of time to allow the elevated host to run.
  base::TimeDelta host_process_timeout_;

  // EventHandler of the parent process.
  extensions::NativeMessageHost::Client* client_;

  // Native messaging channel used to communicate with the elevated host.
  std::unique_ptr<extensions::NativeMessagingChannel> elevated_channel_;

  // Timer to control the lifetime of the elevated host.
  base::OneShotTimer elevated_host_timer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ElevatedNativeMessagingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_ELEVATED_NATIVE_MESSAGING_HOST_H_
