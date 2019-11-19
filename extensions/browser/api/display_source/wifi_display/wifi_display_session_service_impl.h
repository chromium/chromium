// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_SESSION_SERVICE_IMPL_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_SESSION_SERVICE_IMPL_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate.h"
#include "extensions/common/mojom/wifi_display_session_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_reciever.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace extensions {

// This class provides access to the network transport for the Wi-Fi Display
// session (which is itself hosted in the sandboxed renderer process).
class WiFiDisplaySessionServiceImpl
    : public mojom::WiFiDisplaySessionService,
      public DisplaySourceConnectionDelegate::Observer {
 public:
  ~WiFiDisplaySessionServiceImpl() override;
  static void BindToReceiver(
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<mojom::WiFiDisplaySessionService> receiver,
      content::RenderFrameHost* render_frame_host);

 private:
  // WiFiDisplaySessionService overrides.
  void SetClient(mojo::PendingRemote<mojom::WiFiDisplaySessionServiceClient>
                     client) override;
  void Connect(int32_t sink_id,
               int32_t auth_method,
               const std::string& auth_data) override;
  void Disconnect() override;
  void SendMessage(const std::string& message) override;

  // DisplaySourceConnectionDelegate::Observer overrides.
  void OnSinksUpdated(const DisplaySourceSinkInfoList& sinks) override;
  void OnConnectionError(int sink_id,
                         DisplaySourceErrorType type,
                         const std::string& description) override;

  explicit WiFiDisplaySessionServiceImpl(
      DisplaySourceConnectionDelegate* delegate);

  // Called if a message is received from the connected sink.
  void OnSinkMessage(const std::string& message);

  // Failure callbacks for Connect and Disconnect methods.
  void OnConnectFailed(int sink_id, const std::string& reason);
  void OnDisconnectFailed(int sink_id, const std::string& reason);

  // Mojo error callback.
  void OnClientConnectionError();

  mojo::Remote<mojom::WiFiDisplaySessionServiceClient> client_;
  DisplaySourceConnectionDelegate* delegate_;

  api::display_source::SinkState sink_state_;
  // Id of the sink of the session this object is associated with.
  int sink_id_;

  mojo::SelfOwnedReceiverRef<mojom::WiFiDisplaySessionService> receiver_;
  base::WeakPtrFactory<WiFiDisplaySessionServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiFiDisplaySessionServiceImpl);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_SESSION_SERVICE_IMPL_H_
