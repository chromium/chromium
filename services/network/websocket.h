// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_H_
#define SERVICES_NETWORK_WEBSOCKET_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_event_interface.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/websocket_throttler.h"
#include "url/origin.h"

class GURL;

namespace base {
class Location;
}  // namespace base

namespace net {
class IOBuffer;
class IsolationInfo;
class SSLInfo;
class SiteForCookies;
class WebSocketChannel;
}  // namespace net

namespace network {

class WebSocketFactory;

// Host of net::WebSocketChannel.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebSocket : public mojom::WebSocket {
 public:
  using HasRawHeadersAccess =
      base::StrongAlias<class HasRawHeadersAccessTag, bool>;

  WebSocket(
      WebSocketFactory* factory,
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      const net::SiteForCookies& site_for_cookies,
      const net::IsolationInfo& isolation_info,
      std::vector<mojom::HttpHeaderPtr> additional_headers,
      const url::Origin& origin,
      uint32_t options,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      HasRawHeadersAccess has_raw_cookie_access,
      mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
      mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver>
          auth_cert_observer,
      mojo::PendingRemote<mojom::WebSocketAuthenticationHandler> auth_handler,
      mojo::PendingRemote<mojom::TrustedHeaderClient> header_client,
      base::Optional<WebSocketThrottler::PendingConnection>
          pending_connection_tracker,
      DataPipeUseTracker,
      base::TimeDelta delay);
  ~WebSocket() override;

  // mojom::WebSocket methods:
  void SendMessage(mojom::WebSocketMessageType type,
                   uint64_t data_length) override;
  void StartReceiving() override;
  void StartClosingHandshake(uint16_t code, const std::string& reason) override;

  // Whether to allow sending/setting cookies during WebSocket handshakes for
  // |url|. This decision is based on the |options_| and |origin_| this
  // WebSocket was created with.
  bool AllowCookies(const GURL& url) const;

  // These methods are called by the network delegate to forward these events to
  // the |header_client_|.
  int OnBeforeStartTransaction(net::CompletionOnceCallback callback,
                               net::HttpRequestHeaders* headers);
  int OnHeadersReceived(
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      base::Optional<GURL>* preserve_fragment_on_redirect_url);

  // Gets the WebSocket associated with this request.
  static WebSocket* ForRequest(const net::URLRequest& request);

  static const void* const kUserDataKey;

 private:
  class WebSocketEventHandler;
  struct CloseInfo;

  // This class is used to set the WebSocket as user data on a URLRequest. This
  // is used instead of WebSocket directly because SetUserData requires a
  // std::unique_ptr. This is safe because WebSocket owns the URLRequest, so is
  // guaranteed to outlive it.
  class UnownedPointer : public base::SupportsUserData::Data {
   public:
    explicit UnownedPointer(WebSocket* pointer) : pointer_(pointer) {}

    WebSocket* get() const { return pointer_; }

   private:
    WebSocket* const pointer_;

    DISALLOW_COPY_AND_ASSIGN(UnownedPointer);
  };

  struct DataFrame final {
    DataFrame(mojom::WebSocketMessageType type,
              uint64_t data_length,
              bool do_not_fragment)
        : type(type),
          data_length(data_length),
          do_not_fragment(do_not_fragment) {}
    mojom::WebSocketMessageType type;
    uint64_t data_length;
    const bool do_not_fragment;
  };

  void OnConnectionError(const base::Location& set_from);
  void AddChannel(const GURL& socket_url,
                  const std::vector<std::string>& requested_protocols,
                  const net::SiteForCookies& site_for_cookies,
                  const net::IsolationInfo& isolation_info,
                  std::vector<mojom::HttpHeaderPtr> additional_headers);
  void OnSSLCertificateErrorResponse(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          callbacks,
      const net::SSLInfo& ssl_info,
      int net_error);
  void OnAuthRequiredComplete(
      base::OnceCallback<void(const net::AuthCredentials*)> callback,
      const base::Optional<net::AuthCredentials>& credential);
  void OnBeforeSendHeadersComplete(
      net::CompletionOnceCallback callback,
      net::HttpRequestHeaders* out_headers,
      int result,
      const base::Optional<net::HttpRequestHeaders>& headers);
  void OnHeadersReceivedComplete(
      net::CompletionOnceCallback callback,
      scoped_refptr<net::HttpResponseHeaders>* out_headers,
      base::Optional<GURL>* out_preserve_fragment_on_redirect_url,
      int result,
      const base::Optional<std::string>& headers,
      const base::Optional<GURL>& preserve_fragment_on_redirect_url);

  void Reset();

  // Datapipe functions to receive.
  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state);
  void SendPendingDataFrames();
  void SendDataFrame(base::span<const char>* data_span);

  // Datapipe functions to send.
  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);

  // ReadAndSendFromDataPipe() may indirectly delete |this|.
  void ReadAndSendFromDataPipe();
  void ResumeDataPipeReading();

  // |factory_| owns |this|.
  WebSocketFactory* const factory_;
  mojo::Receiver<mojom::WebSocket> receiver_{this};

  mojo::Remote<mojom::AuthenticationAndCertificateObserver> auth_cert_observer_;
  mojo::Remote<mojom::WebSocketHandshakeClient> handshake_client_;
  mojo::Remote<mojom::WebSocketClient> client_;
  mojo::Remote<mojom::WebSocketAuthenticationHandler> auth_handler_;
  mojo::Remote<mojom::TrustedHeaderClient> header_client_;

  base::Optional<WebSocketThrottler::PendingConnection>
      pending_connection_tracker_;

  // The channel we use to send events to the network.
  std::unique_ptr<net::WebSocketChannel> channel_;

  // Delay used for per-renderer WebSocket throttling.
  const base::TimeDelta delay_;

  const uint32_t options_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // The web origin to use for the WebSocket.
  const url::Origin origin_;

  // For 3rd-party cookie permission checking.
  net::SiteForCookies site_for_cookies_;

  bool handshake_succeeded_ = false;
  const HasRawHeadersAccess has_raw_headers_access_;

  // Datapipe fields to receive.
  mojo::ScopedDataPipeProducerHandle writable_;
  mojo::SimpleWatcher writable_watcher_;
  base::queue<base::span<const char>> pending_data_frames_;
  bool wait_for_writable_ = false;

  // Datapipe fields to send.
  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  base::queue<DataFrame> pending_send_data_frames_;
  bool wait_for_readable_ = false;
  bool blocked_on_websocket_channel_ = false;

  DataPipeUseTracker data_pipe_use_tracker_;

  // True if we should preserve the old behaviour where <=64KB messages were
  // never fragmented.
  // TODO(ricea): Remove the flag once we know whether we really need this or
  // not. See https://crbug.com/1086273.
  const bool reassemble_short_messages_;

  // Temporary buffer for storage of short messages that have been fragmented by
  // the data pipe. Only messages that are actually fragmented are copied into
  // here.
  scoped_refptr<net::IOBuffer> message_under_reassembly_;

  // Number of bytes that have been written to |message_under_reassembly_| so
  // far.
  size_t bytes_reassembled_ = 0;

  // Set when StartClosingHandshake() is called while
  // |pending_send_data_frames_| is non-empty. This can happen due to a race
  // condition between the readable signal on the data pipe and the channel on
  // which StartClosingHandshake() is called.
  std::unique_ptr<CloseInfo> pending_start_closing_handshake_;

  base::WeakPtrFactory<WebSocket> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEBSOCKET_H_
