// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVICE_BOUND_SESSION_SERVICE_DELEGATE_H_
#define SERVICES_NETWORK_DEVICE_BOUND_SESSION_SERVICE_DELEGATE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/device_bound_sessions/session_service.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace net {
class X509Certificate;
class SSLPrivateKey;
}  // namespace net

namespace network {

// Delegate class that implements mojom::ClientCertificateResponder.
// It receives client certificate requests from Device Bound Session Service and
// routes them to the browser process via URLLoaderNetworkServiceObserver.
// It handles responses asynchronously and responds to the Network Service
// using the ClientCertificateResponder interface.
//
// Note: This object can be constructed with a null (or unbound)
// `url_loader_network_observer`. In that case, `SelectClientCertificate`
// immediately invokes the callback with `cancel = true`, causing the
// underlying network request to be cancelled with
// `ERR_SSL_CLIENT_AUTH_CERT_NEEDED`.
class COMPONENT_EXPORT(NETWORK_SERVICE) DeviceBoundSessionServiceDelegate
    : public mojom::ClientCertificateResponder {
 public:
  explicit DeviceBoundSessionServiceDelegate(
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer);
  ~DeviceBoundSessionServiceDelegate() override;

  DeviceBoundSessionServiceDelegate(const DeviceBoundSessionServiceDelegate&) =
      delete;
  DeviceBoundSessionServiceDelegate& operator=(
      const DeviceBoundSessionServiceDelegate&) = delete;

  // Selects a client certificate and forwards it to the browser.
  void SelectClientCertificate(
      const GURL& url,
      scoped_refptr<net::SSLCertRequestInfo> cert_info,
      net::device_bound_sessions::SelectClientCertificateCallback callback);

  // mojom::ClientCertificateResponder:
  void ContinueWithCertificate(
      const scoped_refptr<net::X509Certificate>& x509_certificate,
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key) override;
  void ContinueWithoutCertificate() override;
  void CancelRequest() override;

 private:
  void OnReceiverDisconnected();

  mojo::Remote<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer_;

  mojo::ReceiverSet<mojom::ClientCertificateResponder,
                    net::device_bound_sessions::SelectClientCertificateCallback>
      receivers_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVICE_BOUND_SESSION_SERVICE_DELEGATE_H_
