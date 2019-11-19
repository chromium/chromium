// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session_details.h"

#if defined(OS_WIN)
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#endif  // defined(OS_WIN)

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
    const base::Closure& connect_callback,
    const base::Closure& done_callback)
    : connection_id_(connection_id),
      client_session_details_(client_session_details),
      initial_connect_timeout_(initial_connect_timeout),
      connect_callback_(connect_callback),
      done_callback_(done_callback),
      message_callback_(message_callback) {
  DCHECK_GT(connection_id_, 0);
  DCHECK(!done_callback_.is_null());
  DCHECK(!message_callback_.is_null());
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
#if defined(OS_WIN)
  options.enforce_uniqueness = false;
  // Create a named pipe owned by the current user (the LocalService account
  // (SID: S-1-5-19) when running in the network process) which is available to
  // all authenticated users.
  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    return false;
  }
  std::string user_sid_utf8 = base::WideToUTF8(user_sid);
  options.security_descriptor = base::UTF8ToUTF16(base::StringPrintf(
      "O:%sG:%sD:(A;;GA;;;AU)", user_sid_utf8.c_str(), user_sid_utf8.c_str()));

#endif  // defined(OS_WIN)
  mojo::NamedPlatformChannel channel(options);

  mojo_connection_ = std::make_unique<mojo::IsolatedConnection>();
  ipc_channel_ = IPC::Channel::CreateServer(
      mojo_connection_->Connect(channel.TakeServerEndpoint()).release(), this,
      base::ThreadTaskRunnerHandle::Get());

  if (!ipc_channel_->Connect()) {
    ipc_channel_.reset();
    return false;
  }
  // It is safe to use base::Unretained here as |timer_| will be stopped and
  // this task will be removed when this instance is being destroyed.  All
  // methods must execute on the same thread (due to |thread_Checker_| so
  // the posted task and D'Tor can not execute concurrently.
  timer_.Start(FROM_HERE, initial_connect_timeout_,
               base::Bind(&SecurityKeyIpcServerImpl::OnChannelError,
                          base::Unretained(this)));
  return true;
}

bool SecurityKeyIpcServerImpl::SendResponse(const std::string& response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Since we have received a response, we update the timer and wait
  // for a subsequent request.
  timer_.Start(FROM_HERE, security_key_request_timeout_,
               base::Bind(&SecurityKeyIpcServerImpl::OnChannelError,
                          base::Unretained(this)));

  return ipc_channel_->Send(
      new ChromotingNetworkToRemoteSecurityKeyMsg_Response(response));
}

bool SecurityKeyIpcServerImpl::OnMessageReceived(const IPC::Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (connection_close_pending_) {
    LOG(WARNING) << "IPC Message ignored because channel is being closed.";
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SecurityKeyIpcServerImpl, message)
    IPC_MESSAGE_HANDLER(ChromotingRemoteSecurityKeyToNetworkMsg_Request,
                        OnSecurityKeyRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
}

void SecurityKeyIpcServerImpl::OnChannelConnected(int32_t peer_pid) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!connect_callback_.is_null()) {
    std::move(connect_callback_).Run();
  }

#if defined(OS_WIN)
  DWORD peer_session_id;
  if (!ProcessIdToSessionId(peer_pid, &peer_session_id)) {
    PLOG(ERROR) << "ProcessIdToSessionId() failed";
    connection_close_pending_ = true;
  } else if (peer_session_id != client_session_details_->desktop_session_id()) {
    LOG(ERROR) << "Ignoring connection attempt from outside remoted session.";
    connection_close_pending_ = true;
  }
  if (connection_close_pending_) {
    ipc_channel_->Send(
        new ChromotingNetworkToRemoteSecurityKeyMsg_InvalidSession());

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SecurityKeyIpcServerImpl::OnChannelError,
                                  weak_factory_.GetWeakPtr()));
    return;
  }
#else   // !defined(OS_WIN)
  CHECK_EQ(client_session_details_->desktop_session_id(), UINT32_MAX);
#endif  // !defined(OS_WIN)

  // Reset the timer to give the client a chance to send the request.
  timer_.Start(FROM_HERE, initial_connect_timeout_,
               base::Bind(&SecurityKeyIpcServerImpl::OnChannelError,
                          base::Unretained(this)));

  ipc_channel_->Send(
      new ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionReady());
}

void SecurityKeyIpcServerImpl::OnChannelError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CloseChannel();

  if (!connect_callback_.is_null()) {
    std::move(connect_callback_).Run();
  }
  if (!done_callback_.is_null()) {
    // Note: This callback may result in this object being torn down.
    std::move(done_callback_).Run();
  }
}

void SecurityKeyIpcServerImpl::OnSecurityKeyRequest(
    const std::string& request_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Reset the timer to give the client a chance to send the response.
  timer_.Start(FROM_HERE, security_key_request_timeout_,
               base::Bind(&SecurityKeyIpcServerImpl::OnChannelError,
                          base::Unretained(this)));

  HOST_LOG << "Received security key request: " << GetCommandCode(request_data);
  message_callback_.Run(connection_id_, request_data);
}

void SecurityKeyIpcServerImpl::CloseChannel() {
  if (ipc_channel_) {
    ipc_channel_->Close();
    connection_close_pending_ = false;
  }
  mojo_connection_.reset();
}

}  // namespace remoting
