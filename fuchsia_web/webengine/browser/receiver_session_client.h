// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_RECEIVER_SESSION_CLIENT_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_RECEIVER_SESSION_CLIENT_H_

#include <fuchsia/web/cpp/fidl.h>

#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace cast_streaming {
class ReceiverSession;
}  // namespace cast_streaming

// Holds a Cast Streaming Receiver Session.
class ReceiverSessionClient {
 public:
  explicit ReceiverSessionClient(
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
      bool video_only_receiver);
  ReceiverSessionClient(const ReceiverSessionClient& other) = delete;

  ~ReceiverSessionClient();

  ReceiverSessionClient& operator=(const ReceiverSessionClient&) = delete;

  void SetMojoEndpoints(
      mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
          demuxer_connector,
      mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
          renderer_controller);

  bool HasReceiverSession();

 private:
  // Populated in the ctor, and removed when |receiver_session_| is created in
  // SetDemuxerConnector().
  fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request_;

  // Created in SetDemuxerConnector(), and empty prior to that call.
  std::unique_ptr<cast_streaming::ReceiverSession> receiver_session_;

  const bool video_only_receiver_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_RECEIVER_SESSION_CLIENT_H_
