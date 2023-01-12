// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jamiewalch): Add unit tests for this.

#include "remoting/host/posix/signal_handler.h"

#include <errno.h>
#include <signal.h>

#include <list>
#include <memory>
#include <tuple>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/platform_thread.h"

namespace remoting {
namespace {

int g_read_fd = 0;
int g_write_fd = 0;

class SignalListener {
 public:
  SignalListener();

  void AddSignalHandler(int signal, const SignalHandler& handler);

  void OnFileCanReadWithoutBlocking();

  // Throughout the lifetime of this, OnFileCanReadWithoutBlocking() is called
  // whenever data is available in |g_read_fd|.
  std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;

 private:
  typedef std::pair<int, SignalHandler> SignalAndHandler;
  typedef std::list<SignalAndHandler> SignalHandlers;
  SignalHandlers signal_handlers_;
};

SignalListener::SignalListener() = default;

void SignalListener::AddSignalHandler(int signal,
                                      const SignalHandler& handler) {
  signal_handlers_.push_back(SignalAndHandler(signal, handler));
}

void SignalListener::OnFileCanReadWithoutBlocking() {
  char buffer;
  int result = HANDLE_EINTR(read(g_read_fd, &buffer, sizeof(buffer)));
  if (result > 0) {
    for (SignalHandlers::const_iterator i = signal_handlers_.begin();
         i != signal_handlers_.end(); ++i) {
      if (i->first == buffer) {
        i->second.Run(i->first);
      }
    }
  }
}

SignalListener* g_signal_listener = nullptr;

void GlobalSignalHandler(int signal) {
  char byte = signal;
  std::ignore = write(g_write_fd, &byte, 1);
}

}  // namespace

// RegisterSignalHandler registers a signal handler that writes a byte to a
// pipe each time a signal is received. The read end of the pipe is registered
// with the current MessageLoop (which must be of type IO); whenever the pipe
// is readable, it invokes the specified callback.
//
// This arrangement is required because the set of system APIs that are safe to
// call from a signal handler is very limited (but does include write).
bool RegisterSignalHandler(int signal_number, const SignalHandler& handler) {
  CHECK(signal_number < 256);  // Don't want to worry about multi-byte writes.
  if (!g_signal_listener) {
    g_signal_listener = new SignalListener();
  }
  if (!g_write_fd) {
    int pipe_fd[2];
    int result = pipe(pipe_fd);
    if (result < 0) {
      PLOG(ERROR) << "Could not create signal pipe";
      return false;
    }

    g_read_fd = pipe_fd[0];
    g_write_fd = pipe_fd[1];

    g_signal_listener->controller = base::FileDescriptorWatcher::WatchReadable(
        g_read_fd,
        base::BindRepeating(&SignalListener::OnFileCanReadWithoutBlocking,
                            base::Unretained(g_signal_listener)));
  }
  if (signal(signal_number, GlobalSignalHandler) == SIG_ERR) {
    PLOG(ERROR) << "signal() failed";
    return false;
  }
  g_signal_listener->AddSignalHandler(signal_number, handler);
  return true;
}

}  // namespace remoting
