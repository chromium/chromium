// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/device_bound_session_service_delegate.h"

#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "services/network/ssl_private_key_proxy.h"

namespace network {

DeviceBoundSessionServiceDelegate::DeviceBoundSessionServiceDelegate(
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer) {
  if (url_loader_network_observer) {
    url_loader_network_observer_.Bind(std::move(url_loader_network_observer));
  }
  receivers_.set_disconnect_handler(base::BindRepeating(
      &DeviceBoundSessionServiceDelegate::OnReceiverDisconnected,
      base::Unretained(this)));
}

DeviceBoundSessionServiceDelegate::~DeviceBoundSessionServiceDelegate() =
    default;

void DeviceBoundSessionServiceDelegate::SelectClientCertificate(
    const GURL& url,
    scoped_refptr<net::SSLCertRequestInfo> cert_info,
    net::device_bound_sessions::SelectClientCertificateCallback callback) {
  if (!url_loader_network_observer_ ||
      !url_loader_network_observer_.is_bound()) {
    std::move(callback).Run(nullptr, nullptr, /*cancel=*/true);
    return;
  }

  mojo::PendingRemote<mojom::ClientCertificateResponder> cert_responder;
  receivers_.Add(this, cert_responder.InitWithNewPipeAndPassReceiver(),
                 std::move(callback));

  url_loader_network_observer_->OnCertificateRequested(
      std::nullopt, std::move(cert_info), std::move(cert_responder));
}

void DeviceBoundSessionServiceDelegate::ContinueWithCertificate(
    const scoped_refptr<net::X509Certificate>& x509_certificate,
    const std::string& provider_name,
    const std::vector<uint16_t>& algorithm_preferences,
    mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key) {
  auto callback = std::move(receivers_.current_context());
  receivers_.Remove(receivers_.current_receiver());

  scoped_refptr<net::SSLPrivateKey> proxy_key =
      base::MakeRefCounted<SSLPrivateKeyProxy>(
          provider_name, algorithm_preferences, std::move(ssl_private_key));
  std::move(callback).Run(x509_certificate, std::move(proxy_key),
                          /*cancel=*/false);
}

void DeviceBoundSessionServiceDelegate::ContinueWithoutCertificate() {
  auto callback = std::move(receivers_.current_context());
  receivers_.Remove(receivers_.current_receiver());
  std::move(callback).Run(nullptr, nullptr, /*cancel=*/false);
}

void DeviceBoundSessionServiceDelegate::CancelRequest() {
  auto callback = std::move(receivers_.current_context());
  receivers_.Remove(receivers_.current_receiver());
  std::move(callback).Run(nullptr, nullptr, /*cancel=*/true);
}

void DeviceBoundSessionServiceDelegate::OnReceiverDisconnected() {
  auto callback = std::move(receivers_.current_context());
  std::move(callback).Run(nullptr, nullptr, /*cancel=*/true);
}

}  // namespace network
