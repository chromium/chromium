// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "remoting/host/mojo_ipc_server.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"
#include "remoting/host/remote_open_url_constants.h"
#include "remoting/proto/remote_open_url.pb.h"
#include "remoting/protocol/named_message_pipe_handler.h"

class GURL;

namespace remoting {

class RemoteOpenUrlMessageHandler final
    : public mojom::RemoteUrlOpener,
      public protocol::NamedMessagePipeHandler {
 public:
  RemoteOpenUrlMessageHandler(const std::string& name,
                              std::unique_ptr<protocol::MessagePipe> pipe);
  ~RemoteOpenUrlMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

  RemoteOpenUrlMessageHandler(const RemoteOpenUrlMessageHandler&) = delete;
  RemoteOpenUrlMessageHandler& operator=(const RemoteOpenUrlMessageHandler&) =
      delete;

 private:
  void OnIpcDisconnected();

  // RemoteUrlOpener implementation.
  void OpenUrl(const GURL& url, OpenUrlCallback callback) override;

  SEQUENCE_CHECKER(sequence_checker_);

  MojoIpcServer<mojom::RemoteUrlOpener> ipc_server_{
      GetRemoteOpenUrlIpcChannelName(), 0, this};

  static_assert(
      std::is_same<
          mojo::ReceiverId,
          decltype(std::declval<protocol::RemoteOpenUrl::OpenUrlRequest>()
                       .id())>::value,
      "mojo::ReceiverId must have the same type as the |id| field of "
      "OpenUrlRequest.");

  // Receiver ID => OpenUrl callback mapping.
  base::flat_map<mojo::ReceiverId, OpenUrlCallback> callbacks_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_
