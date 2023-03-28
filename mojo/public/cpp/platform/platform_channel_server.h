// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_SERVER_H_
#define MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_SERVER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace mojo {

// PlatformChannelServer takes ownership of a PlatformChannelServerEndpoint
// and listens for a single incoming client connection.
//
// This class is not thread-safe and must be used on a thread which runs an I/O
// MessagePump.
class COMPONENT_EXPORT(MOJO_CPP_PLATFORM) PlatformChannelServer {
 public:
  using ConnectionCallback = base::OnceCallback<void(PlatformChannelEndpoint)>;

  // Implemented for each supported platform.
  class COMPONENT_EXPORT(MOJO_CPP_PLATFORM) Listener {
   public:
    virtual ~Listener() = default;

    // Implemented for each supported platform.
    static std::unique_ptr<Listener> Create();

    // Attempts to start listening on `server_endpoint`. Returns true on success
    // or false on failure. Same semantics as Listen() below, which calls this.
    virtual bool Start(PlatformChannelServerEndpoint& server_endpoint,
                       ConnectionCallback& callback) = 0;
  };

  PlatformChannelServer();

  // Destruction implicitly stops the listener if started, ensuring the
  // ConnectionCallback will not be called beyoned the lifetime of this object.
  ~PlatformChannelServer();

  // Spins up a PlatformChannelServer on the current (I/O) task runner to listen
  // on `server_endpoint` for an incoming connection. `callback` is always
  // called eventually, as long as the calling task runner is still running when
  // either the server accepts a connection or is disconnected. If disconnected,
  // `callback` receives an invalid endpoint.
  static void WaitForConnection(PlatformChannelServerEndpoint server_endpoint,
                                ConnectionCallback callback);

  // Listens on `server_endpoint` for a single connection, invoking `callback`
  // once it arrives. Must not be called on a server that is already listening.
  //
  // If the server endpoint is disconnected before a connection is received,
  // the callback will be invoked with an invalid endpoint.
  //
  // If the server is stopped before a connection is received, `callback` will
  // not be called.
  //
  // If the server could not listen on the given endpoint, this returns false
  // and `callback` is never called. Otherwise it returns true.
  //
  // This takes ownership of (i.e. moves) `server_endpoint` and `callback` if
  // and only if it returns true.
  bool TryListen(PlatformChannelServerEndpoint& server_endpoint,
                 ConnectionCallback& callback);

  // Same as above, but takes a callback by value for convenience when the
  // doesn't care about retaining the arguments in the failure case.
  bool Listen(PlatformChannelServerEndpoint server_endpoint,
              ConnectionCallback callback) {
    return TryListen(server_endpoint, callback);
  }

  // Stops listening for new connections immediately. The callback given to
  // Listen() can no longer be invoked once this is called.
  void Stop();

 private:
  std::unique_ptr<Listener> listener_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_SERVER_H_
