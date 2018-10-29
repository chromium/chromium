// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/scoped_android_configuration.h"

#include <fcntl.h>
#include <iostream>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/android/url_utils.h"
#include "base/test/test_support_android.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/socket/socket_posix.h"
#include "url/gurl.h"

using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

void ConnectCompleted(const base::Closure& socket_connected, int rv) {
  LOG_IF(FATAL, net::OK != rv) << " Failed to redirect to socket: "
                               << net::ErrorToString(rv);
  socket_connected.Run();
}

void CreateAndConnectSocket(
    uint16_t port,
    const base::Callback<void(std::unique_ptr<net::SocketPosix>)>&
        socket_connected) {
  net::SockaddrStorage storage;
  net::IPAddress address;
  LOG_IF(FATAL, !address.AssignFromIPLiteral("127.0.0.1"))
      << "Failed to create IPAddress from IP literal 127.0.0.1.";
  net::IPEndPoint endpoint(address, port);
  LOG_IF(FATAL, !endpoint.ToSockAddr(storage.addr, &storage.addr_len))
      << "Failed to convert " << endpoint.ToString() << " to sockaddr.";

  std::unique_ptr<net::SocketPosix> socket(
      std::make_unique<net::SocketPosix>());

  int result = socket->Open(AF_INET);
  LOG_IF(FATAL, net::OK != result) << "Failed to open socket for "
                                   << endpoint.ToString() << ": "
                                   << net::ErrorToString(result);

  // Set the socket as blocking.
  const int flags = fcntl(socket->socket_fd(), F_GETFL);
  LOG_IF(FATAL, flags == -1);
  if (flags & O_NONBLOCK) {
    fcntl(socket->socket_fd(), F_SETFL, flags & ~O_NONBLOCK);
  }

  net::SocketPosix* socket_ptr = socket.get();
  net::CompletionCallback connect_completed =
      base::Bind(&ConnectCompleted,
                 base::Bind(socket_connected, base::Passed(std::move(socket))));
  result = socket_ptr->Connect(storage, connect_completed);
  if (result != net::ERR_IO_PENDING) {
    connect_completed.Run(result);
  }
}

void RedirectStdout(int fd) {
  LOG_IF(FATAL, dup2(fd, STDOUT_FILENO) == -1) << "Failed to dup2 stdout: "
                                               << strerror(errno);
}

void RedirectStdin(int fd) {
  LOG_IF(FATAL, dup2(fd, STDIN_FILENO) == -1) << "Failed to dup2 stdin: "
                                              << strerror(errno);
}

void RedirectStderr(int fd) {
  LOG_IF(FATAL, dup2(fd, STDERR_FILENO) == -1) << "Failed to dup2 stderr: "
                                               << strerror(errno);
}

void FinishRedirection(
    const base::Callback<void(int)>& redirect,
    const base::Callback<void(std::unique_ptr<net::SocketPosix>)>&
        transfer_socket,
    base::WaitableEvent* event,
    std::unique_ptr<net::SocketPosix> socket) {
  redirect.Run(socket->socket_fd());
  transfer_socket.Run(std::move(socket));
  event->Signal();
}

void RedirectStream(
    uint16_t port,
    const base::Callback<void(base::WaitableEvent*,
                              std::unique_ptr<net::SocketPosix>)>&
        finish_redirection) {
  base::WaitableEvent redirected(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(&CreateAndConnectSocket, port,
                 base::Bind(finish_redirection, &redirected)));
  ScopedAllowWaitForAndroidLayoutTests allow_wait;
  while (!redirected.IsSignaled())
    redirected.Wait();
}

}  // namespace

ScopedAndroidConfiguration::ScopedAndroidConfiguration() : sockets_() {
  base::InitAndroidTestPaths(base::android::GetIsolatedTestRoot());
}

ScopedAndroidConfiguration::~ScopedAndroidConfiguration() = default;

void ScopedAndroidConfiguration::RedirectStreams() {
  // Unretained is safe here because all executions of add_socket finish
  // before this function returns.
  base::Callback<void(std::unique_ptr<net::SocketPosix>)> add_socket =
      base::Bind(&ScopedAndroidConfiguration::AddSocket,
                 base::Unretained(this));

  std::string stdout_port_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kAndroidStdoutPort);
  unsigned stdout_port = 0;
  if (base::StringToUint(stdout_port_str, &stdout_port)) {
    RedirectStream(base::checked_cast<uint16_t>(stdout_port),
                   base::Bind(&FinishRedirection, base::Bind(&RedirectStdout),
                              add_socket));
  }

  std::string stdin_port_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kAndroidStdinPort);
  unsigned stdin_port = 0;
  if (base::StringToUint(stdin_port_str, &stdin_port)) {
    RedirectStream(
        base::checked_cast<uint16_t>(stdin_port),
        base::Bind(&FinishRedirection, base::Bind(&RedirectStdin), add_socket));
  }

  std::string stderr_port_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kAndroidStderrPort);
  unsigned stderr_port = 0;
  if (base::StringToUint(stderr_port_str, &stderr_port)) {
    RedirectStream(base::checked_cast<uint16_t>(stderr_port),
                   base::Bind(&FinishRedirection, base::Bind(&RedirectStderr),
                              add_socket));
  }
}

void ScopedAndroidConfiguration::AddSocket(
    std::unique_ptr<net::SocketPosix> socket) {
  sockets_.push_back(std::move(socket));
}

}  // namespace content
