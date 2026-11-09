// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_FAKE_MCS_SERVER_H_
#define GOOGLE_APIS_GCM_ENGINE_FAKE_MCS_SERVER_H_

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "url/gurl.h"

namespace gcm {

// A fake Mobile Connection Server (MCS) speaking the binary MCS streaming
// protocol over a local TCP socket, acting as the server-side counterpart to
// `gcm::MCSClient`.
//
// Handles `LoginRequest`/`LoginResponse`, `HeartbeatPing`/`HeartbeatAck`,
// `DataMessageStanza` framing, RMQ2 `received_persistent_id` / `StreamAck`
// tracking, and optional state persistence across `PRE_` restarts.
//
// Note: `FakeMCSServer` intentionally avoids sharing client-side packet
// parsing and protobuf field access helpers from `mcs_util.h` / `mcs_message.h`
// so that client bugs are not masked by symmetric server behavior in tests.
class FakeMCSServer {
 public:
  using DataMessageReceivedCallback =
      base::RepeatingCallback<void(uint64_t android_id,
                                   const mcs_proto::DataMessageStanza& stanza)>;

  FakeMCSServer();
  FakeMCSServer(const FakeMCSServer&) = delete;
  FakeMCSServer& operator=(const FakeMCSServer&) = delete;
  ~FakeMCSServer();

  // Binds the local TCP listening socket so `GetEndpointURL()` can be queried
  // before `Start()` is called.
  void InitializeAndListen();

  // Loads any persisted state from `storage_dir` (if non-empty) and starts the
  // background IO thread to accept client connections. `InitializeAndListen()`
  // must be called before `Start()`.
  void Start(const base::FilePath& storage_dir = base::FilePath());

  // Returns the local URL (`http://<address>:<port>`) where the binary MCS TCP
  // server is listening. `InitializeAndListen()` must be called first.
  GURL GetEndpointURL() const;

  // Assigns a unique `persistent_id` to `stanza` (if not already populated),
  // queues it for `android_id`, persists state to disk (if configured), and
  // delivers it immediately if `android_id` has an active MCS connection.
  void SendDataMessage(uint64_t android_id,
                       mcs_proto::DataMessageStanza stanza);

  // Registers a callback invoked (on the calling sequence) whenever a client
  // sends an upstream `DataMessageStanza` to the server.
  void SetDataMessageReceivedCallback(DataMessageReceivedCallback callback);

  // Closes all active client TCP connections, simulating a server-side
  // disconnect.
  void DisconnectAllClients();

  // Returns true if `android_id` currently has unacknowledged server-to-client
  // messages in the server's RMQ queue.
  bool HasUnacknowledgedMessages(uint64_t android_id) const;

 private:
  class StateCore;
  class ServerCore;

  scoped_refptr<StateCore> state_core_;
  base::Thread io_thread_;
  std::unique_ptr<ServerCore> server_core_;
  GURL endpoint_url_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_FAKE_MCS_SERVER_H_
