// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_H_
#define SERVICES_NETWORK_WEBSOCKET_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_delegate.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_event_interface.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/websocket_interceptor.h"
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
      net::StorageAccessApiStatus storage_access_api_status,
      const net::IsolationInfo& isolation_info,
      std::vector<mojom::HttpHeaderPtr> additional_headers,
      const url::Origin& origin,
      uint32_t options,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      HasRawHeadersAccess has_raw_cookie_access,
      mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<mojom::WebSocketAuthenticationHandler> auth_handler,
      mojo::PendingRemote<mojom::TrustedHeaderClient> header_client,
      std::optional<WebSocketThrottler::PendingConnection>
          pending_connection_tracker,
      base::TimeDelta delay,
      const std::optional<base::UnguessableToken>& throttling_profile_id);

  WebSocket(const WebSocket&) = delete;
  WebSocket& operator=(const WebSocket&) = delete;

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

  // Returns if the nonce from |isolation_info_| matches |nonce|, which
  // originates from a fenced frame whose network access is being revoked.
  bool RevokeIfNonceMatches(const base::UnguessableToken& nonce);

  // These methods are called by the network delegate to forward these events to
  // the |header_client_|.
  int OnBeforeStartTransaction(
      const net::HttpRequestHeaders& headers,
      net::NetworkDelegate::OnBeforeStartTransactionCallback callback);
  int OnHeadersReceived(
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      std::optional<GURL>* preserve_fragment_on_redirect_url);

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

    UnownedPointer(const UnownedPointer&) = delete;
    UnownedPointer& operator=(const UnownedPointer&) = delete;

    WebSocket* get() const { return pointer_; }

   private:
    const raw_ptr<WebSocket> pointer_;
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
                  net::StorageAccessApiStatus storage_access_api_status,
                  const net::IsolationInfo& isolation_info,
                  std::vector<mojom::HttpHeaderPtr> additional_headers);
  void OnSSLCertificateErrorResponse(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          callbacks,
      const net::SSLInfo& ssl_info,
      int net_error);
  void OnAuthRequiredComplete(
      base::OnceCallback<void(const net::AuthCredentials*)> callback,
      const std::optional<net::AuthCredentials>& credential);
  void OnBeforeSendHeadersComplete(
      net::NetworkDelegate::OnBeforeStartTransactionCallback callback,
      int result,
      const std::optional<net::HttpRequestHeaders>& headers);
  void OnHeadersReceivedComplete(
      net::CompletionOnceCallback callback,
      scoped_refptr<net::HttpResponseHeaders>* out_headers,
      std::optional<GURL>* out_preserve_fragment_on_redirect_url,
      int result,
      const std::optional<std::string>& headers,
      const std::optional<GURL>& preserve_fragment_on_redirect_url);

  void Reset();

  enum class InterruptionReason {
    // Not interrupted or not resuming after interruptions (but processing a
    // brand new frame)
    kNone,
    // Interrupted by empty Mojo pipe or resuming afterwards
    kMojoPipe,
    // Interrupted by the interceptor or resuming afterwards
    kInterceptor,
  };

  // Datapipe functions to receive.
  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state);
  void SendPendingDataFrames(InterruptionReason resume_reason);
  void SendDataFrame(base::span<const char>* data_span);

  // Datapipe functions to send.
  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);

  void ReadAndSendFromDataPipe(InterruptionReason resume_reason);
  // This helper method only called from ReadAndSendFromDataPipe.
  // Note that it may indirectly delete |this|.
  // Returns true if the frame has been sent completely.
  bool ReadAndSendFrameFromDataPipe(DataFrame* data_frame);
  void ResumeDataPipeReading();

  // |factory_| owns |this|.
  const raw_ptr<WebSocketFactory> factory_;
  mojo::Receiver<mojom::WebSocket> receiver_{this};

  mojo::Remote<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer_;
  mojo::Remote<mojom::WebSocketHandshakeClient> handshake_client_;
  mojo::Remote<mojom::WebSocketClient> client_;
  mojo::Remote<mojom::WebSocketAuthenticationHandler> auth_handler_;
  mojo::Remote<mojom::TrustedHeaderClient> header_client_;

  std::optional<WebSocketThrottler::PendingConnection>
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

  // Used by RevokeIfNonceMatches() for handling network revocation.
  const net::IsolationInfo isolation_info_;

  bool handshake_succeeded_ = false;
  const HasRawHeadersAccess has_raw_headers_access_;

  InterruptionReason incoming_frames_interrupted_ = InterruptionReason::kNone;
  InterruptionReason outgoing_frames_interrupted_ = InterruptionReason::kNone;

  // Datapipe fields to receive.
  mojo::ScopedDataPipeProducerHandle writable_;
  mojo::SimpleWatcher writable_watcher_;
  base::queue<base::span<const char>> pending_data_frames_;

  // Datapipe fields to send.
  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  base::queue<DataFrame> pending_send_data_frames_;
  bool blocked_on_websocket_channel_ = false;

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

  const std::optional<base::UnguessableToken> throttling_profile_id_;
  uint32_t net_log_source_id_ = net::NetLogSource::kInvalidId;
  std::unique_ptr<WebSocketInterceptor> frame_interceptor_;

  base::WeakPtrFactory<WebSocket> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEBSOCKET_H_
