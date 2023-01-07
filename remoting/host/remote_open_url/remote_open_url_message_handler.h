// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"
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
  RemoteOpenUrlMessageHandler(const RemoteOpenUrlMessageHandler&) = delete;
  RemoteOpenUrlMessageHandler& operator=(const RemoteOpenUrlMessageHandler&) =
      delete;
  ~RemoteOpenUrlMessageHandler() override;

  // Adds a receiver to the receiver set.
  void AddReceiver(mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver);
  void ClearReceivers();

  // protocol::NamedMessagePipeHandler overrides.
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

  base::WeakPtr<RemoteOpenUrlMessageHandler> GetWeakPtr();

 private:
  friend class RemoteOpenUrlMessageHandlerTest;

  // Adds a receiver and returns the receiver ID of the added receiver. Used by
  // the unit test.
  mojo::ReceiverId AddReceiverAndGetReceiverId(
      mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver);

  void OnIpcDisconnected();

  // RemoteUrlOpener implementation.
  void OpenUrl(const GURL& url, OpenUrlCallback callback) override;

  SEQUENCE_CHECKER(sequence_checker_);

  static_assert(
      std::is_same<
          mojo::ReceiverId,
          decltype(std::declval<protocol::RemoteOpenUrl::OpenUrlRequest>()
                       .id())>::value,
      "mojo::ReceiverId must have the same type as the |id| field of "
      "OpenUrlRequest.");

  mojo::ReceiverSet<mojom::RemoteUrlOpener> receivers_;

  // Receiver ID => OpenUrl callback mapping.
  base::flat_map<mojo::ReceiverId, OpenUrlCallback> callbacks_;

  base::WeakPtrFactory<RemoteOpenUrlMessageHandler> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_
