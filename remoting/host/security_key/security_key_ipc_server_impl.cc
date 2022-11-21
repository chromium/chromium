// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_server_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_details.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

// Returns the command code (the first byte of the data) if it exists, or -1 if
// the data is empty.
unsigned int GetCommandCode(const std::string& data) {
  return data.empty() ? -1 : static_cast<unsigned int>(data[0]);
}

}  // namespace

namespace remoting {

SecurityKeyIpcServerImpl::SecurityKeyIpcServerImpl(
    int connection_id,
    ClientSessionDetails* client_session_details,
    base::TimeDelta initial_connect_timeout,
    const SecurityKeyAuthHandler::SendMessageCallback& message_callback,
    base::OnceClosure connect_callback,
    base::OnceClosure done_callback)
    : connection_id_(connection_id),
      client_session_details_(client_session_details),
      initial_connect_timeout_(initial_connect_timeout),
      connect_callback_(std::move(connect_callback)),
      done_callback_(std::move(done_callback)),
      message_callback_(message_callback) {
  DCHECK_GT(connection_id_, 0);
  DCHECK(done_callback_);
  DCHECK(message_callback_);
}

SecurityKeyIpcServerImpl::~SecurityKeyIpcServerImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CloseChannel();
}

bool SecurityKeyIpcServerImpl::CreateChannel(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    base::TimeDelta request_timeout) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!ipc_channel_);
  security_key_request_timeout_ = request_timeout;

  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name;
#if BUILDFLAG(IS_WIN)
  options.enforce_uniqueness = false;
  // Create a named pipe owned by the current user (the LocalService account
  // (SID: S-1-5-19) when running in the network process) which is available to
  // all authenticated users.
  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    return false;
  }
  options.security_descriptor = base::StringPrintf(
      L"O:%lsG:%lsD:(A;;GA;;;AU)", user_sid.c_str(), user_sid.c_str());

#endif  // BUILDFLAG(IS_WIN)
  mojo::NamedPlatformChannel channel(options);

  mojo_connection_ = std::make_unique<mojo::IsolatedConnection>();
  ipc_channel_ = IPC::Channel::CreateServer(
      mojo_connection_->Connect(channel.TakeServerEndpoint()).release(), this,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  auto* associated_interface_support =
      ipc_channel_->GetAssociatedInterfaceSupport();

  associated_interface_support->AddGenericAssociatedInterface(
      mojom::SecurityKeyForwarder::Name_,
      base::BindRepeating(&SecurityKeyIpcServerImpl::BindAssociatedInterface,
                          base::Unretained(this)));

  if (!ipc_channel_->Connect()) {
    LOG(ERROR) << "IPC connection failed.";
    ipc_channel_.reset();
    return false;
  }
  // It is safe to use base::Unretained here as |timer_| will be stopped and
  // this task will be removed when this instance is being destroyed.  All
  // methods must execute on the same thread (due to |thread_Checker_| so
  // the posted task and D'Tor can not execute concurrently.
  timer_.Start(FROM_HERE, initial_connect_timeout_,
               base::BindOnce(&SecurityKeyIpcServerImpl::OnChannelError,
                              base::Unretained(this)));
  return true;
}

bool SecurityKeyIpcServerImpl::SendResponse(const std::string& response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Since we have received a response, we update the timer and wait
  // for a subsequent request.
  timer_.Start(FROM_HERE, security_key_request_timeout_,
               base::BindOnce(&SecurityKeyIpcServerImpl::OnChannelError,
                              base::Unretained(this)));

  std::move(response_callback_).Run(response);
  return true;
}

bool SecurityKeyIpcServerImpl::OnMessageReceived(const IPC::Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(false) << "Unexpected call to OnMessageReceived: " << message.type();
  return false;
}

void SecurityKeyIpcServerImpl::OnChannelConnected(int32_t peer_pid) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if BUILDFLAG(IS_WIN)
  bool channel_error = false;
  DWORD peer_session_id;
  if (!ProcessIdToSessionId(peer_pid, &peer_session_id)) {
    PLOG(ERROR) << "ProcessIdToSessionId() failed";
    channel_error = true;
  } else if (peer_session_id != client_session_details_->desktop_session_id()) {
    LOG(ERROR) << "Ignoring connection attempt from outside remoted session.";
    channel_error = true;
  }

  if (channel_error) {
    OnChannelError();
    return;
  }
#else   // !BUILDFLAG(IS_WIN)
  CHECK_EQ(client_session_details_->desktop_session_id(), UINT32_MAX);
#endif  // !BUILDFLAG(IS_WIN)

  if (connect_callback_) {
    std::move(connect_callback_).Run();
  }

  // Reset the timer to give the client a chance to send the request.
  timer_.Start(FROM_HERE, initial_connect_timeout_,
               base::BindOnce(&SecurityKeyIpcServerImpl::OnChannelError,
                              base::Unretained(this)));
}

void SecurityKeyIpcServerImpl::OnChannelError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CloseChannel();

  if (connect_callback_) {
    std::move(connect_callback_).Run();
  }
  if (done_callback_) {
    // Note: This callback may result in this object being torn down.
    std::move(done_callback_).Run();
  }
}

void SecurityKeyIpcServerImpl::BindAssociatedInterface(
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (security_key_forwarder_.is_bound()) {
    LOG(ERROR) << "Receiver already bound for associated interface: "
               << mojom::SecurityKeyForwarder::Name_;
    CloseChannel();
    return;
  }

  mojo::PendingAssociatedReceiver<mojom::SecurityKeyForwarder> pending_receiver(
      std::move(handle));
  security_key_forwarder_.Bind(std::move(pending_receiver));
}

void SecurityKeyIpcServerImpl::OnSecurityKeyRequest(
    const std::string& request_data,
    OnSecurityKeyRequestCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (response_callback_) {
    LOG(ERROR) << "Received security key request while waiting for a response";
    CloseChannel();
    return;
  }

  response_callback_ = std::move(callback);

  // Reset the timer to give the client a chance to send the response.
  timer_.Start(FROM_HERE, security_key_request_timeout_,
               base::BindOnce(&SecurityKeyIpcServerImpl::OnChannelError,
                              base::Unretained(this)));

  HOST_LOG << "Received security key request: " << GetCommandCode(request_data);
  message_callback_.Run(connection_id_, request_data);
}

void SecurityKeyIpcServerImpl::CloseChannel() {
  if (ipc_channel_) {
    ipc_channel_->Close();
  }
  ipc_channel_.reset();
  security_key_forwarder_.reset();

  mojo_connection_.reset();
}

}  // namespace remoting
