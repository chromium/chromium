// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_FAKE_IPC_SERVER_H_
#define REMOTING_HOST_MOJO_IPC_FAKE_IPC_SERVER_H_

#include "base/memory/raw_ptr.h"
#include "remoting/host/mojo_ipc/ipc_server.h"

namespace remoting {

class FakeIpcServer final : public IpcServer {
 public:
  // Used to interact with FakeIpcServer after ownership is passed elsewhere.
  struct TestState {
    TestState();
    ~TestState();

    bool is_server_started = false;
    base::RepeatingClosure disconnect_handler;
    mojo::ReceiverId current_receiver = 0u;
    mojo::ReceiverId last_closed_receiver = 0u;
    int32_t current_peer_pid = 0;
  };

  explicit FakeIpcServer(TestState* test_state);
  FakeIpcServer(const FakeIpcServer&) = delete;
  FakeIpcServer& operator=(const FakeIpcServer&) = delete;
  ~FakeIpcServer() override;

  void StartServer() override;
  void StopServer() override;
  void Close(mojo::ReceiverId id) override;
  void set_disconnect_handler(base::RepeatingClosure handler) override;
  mojo::ReceiverId current_receiver() const override;
  base::ProcessId current_peer_pid() const override;

 private:
  raw_ptr<TestState> test_state_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_FAKE_IPC_SERVER_H_
