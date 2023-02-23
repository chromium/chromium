// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/elevated_native_messaging_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"

namespace remoting {

ElevatedNativeMessagingHost::ElevatedNativeMessagingHost(
    const base::FilePath& binary_path,
    intptr_t parent_window_handle,
    bool elevate_process,
    base::TimeDelta host_timeout,
    extensions::NativeMessageHost::Client* client)
    : host_binary_path_(binary_path),
      parent_window_handle_(parent_window_handle),
      elevate_host_process_(elevate_process),
      host_process_timeout_(host_timeout),
      client_(client) {}

ElevatedNativeMessagingHost::~ElevatedNativeMessagingHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ElevatedNativeMessagingHost::OnMessage(const base::Value& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Simply pass along the response from the elevated host to the client.
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);
  client_->PostMessageFromNativeHost(message_json);
}

void ElevatedNativeMessagingHost::OnDisconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_->CloseChannel(std::string());
}

ProcessLaunchResult ElevatedNativeMessagingHost::EnsureElevatedHostCreated() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (elevated_channel_) {
    return PROCESS_LAUNCH_RESULT_SUCCESS;
  }

  base::win::ScopedHandle read_handle;
  base::win::ScopedHandle write_handle;
  ProcessLaunchResult result = LaunchNativeMessagingHostProcess(
      host_binary_path_, parent_window_handle_, elevate_host_process_,
      &read_handle, &write_handle);
  if (result != PROCESS_LAUNCH_RESULT_SUCCESS) {
    return result;
  }

  // Set up the native messaging channel to talk to the elevated host.
  // Note that input for the elevated channel is output for the elevated host.
  elevated_channel_ = std::make_unique<PipeMessagingChannel>(
      base::File(std::move(read_handle)), base::File(std::move(write_handle)));
  elevated_channel_->Start(this);

  if (!host_process_timeout_.is_zero()) {
    elevated_host_timer_.Start(FROM_HERE, host_process_timeout_, this,
                               &ElevatedNativeMessagingHost::DisconnectHost);
  }

  return PROCESS_LAUNCH_RESULT_SUCCESS;
}

void ElevatedNativeMessagingHost::SendMessage(
    const base::Value::Dict& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  elevated_channel_->SendMessage(message);
}

void ElevatedNativeMessagingHost::DisconnectHost() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This will send an EOF to the elevated host, triggering its shutdown.
  elevated_channel_.reset();
}

}  // namespace remoting
