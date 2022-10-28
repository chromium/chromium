// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_LINUX_H_
#define REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_LINUX_H_

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/mojo_ipc/mojo_server_endpoint_connector.h"

namespace remoting {

// Linux implementation for MojoServerEndpointConnector.
class MojoServerEndpointConnectorLinux final
    : public MojoServerEndpointConnector {
 public:
  explicit MojoServerEndpointConnectorLinux(Delegate* delegate);
  MojoServerEndpointConnectorLinux(const MojoServerEndpointConnectorLinux&) =
      delete;
  MojoServerEndpointConnectorLinux& operator=(
      const MojoServerEndpointConnectorLinux&) = delete;
  ~MojoServerEndpointConnectorLinux() override;

  // MojoServerEndpointConnector implementation.
  void Connect(mojo::PlatformChannelServerEndpoint server_endpoint) override;

 private:
  void OnFileCanReadWithoutBlocking();

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);

  // These are only valid/non-null when there is a pending connection.
  // Note that `pending_server_endpoint_` must outlive
  // `read_watcher_controller_`; otherwise a bad file descriptor error will
  // occur at destruction.
  mojo::PlatformChannelServerEndpoint pending_server_endpoint_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      read_watcher_controller_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<MojoServerEndpointConnectorLinux> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_LINUX_H_
