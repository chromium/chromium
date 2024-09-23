// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/dhcp_pac_file_fetcher_mojo.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "net/proxy_resolution/pac_file_fetcher.h"
#include "net/proxy_resolution/pac_file_fetcher_impl.h"
#include "net/url_request/url_request_context.h"

namespace network {

DhcpPacFileFetcherMojo::DhcpPacFileFetcherMojo(
    net::URLRequestContext* url_request_context,
    mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
        dhcp_wpad_url_client) {
  DCHECK(url_request_context);
  CHECK(dhcp_wpad_url_client) << "DhcpWpadUrlClient not set.";
  pac_file_fetcher_ = net::PacFileFetcherImpl::Create(url_request_context);
  dhcp_wpad_url_client_.Bind(std::move(dhcp_wpad_url_client));
}

DhcpPacFileFetcherMojo::~DhcpPacFileFetcherMojo() = default;

int DhcpPacFileFetcherMojo::Fetch(
    std::u16string* utf16_text,
    net::CompletionOnceCallback callback,
    const net::NetLogWithSource& net_log,
    const net::NetworkTrafficAnnotationTag traffic_annotation) {
  CHECK(callback);
  // DhcpPacFileFetcher only allows one Fetch in progress at a time.
  CHECK(!callback_);
  callback_ = std::move(callback);
  utf16_text_ = utf16_text;
  traffic_annotation_ =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);
  dhcp_wpad_url_client_->GetPacUrl(
      base::BindOnce(&DhcpPacFileFetcherMojo::OnPacUrlReceived,
                     weak_ptr_factory_.GetWeakPtr()));

  return net::ERR_IO_PENDING;
}

void DhcpPacFileFetcherMojo::Cancel() {
  callback_.Reset();
  pac_file_fetcher_->Cancel();
  utf16_text_ = nullptr;

  // Invalidate any pending callbacks (i.e. calls to ContinueFetch).
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DhcpPacFileFetcherMojo::OnShutdown() {
  callback_.Reset();
  pac_file_fetcher_->OnShutdown();
}

const GURL& DhcpPacFileFetcherMojo::GetPacURL() const {
  return pac_url_;
}

std::string DhcpPacFileFetcherMojo::GetFetcherName() const {
  return "with delegate";
}

void DhcpPacFileFetcherMojo::SetPacFileFetcherForTesting(
    std::unique_ptr<net::PacFileFetcher> pac_file_fetcher) {
  pac_file_fetcher_ = std::move(pac_file_fetcher);
}

void DhcpPacFileFetcherMojo::OnPacUrlReceived(const std::string& pac_url) {
  DCHECK(utf16_text_);
  std::u16string* utf16_text = utf16_text_;
  // Clear raw pointer, as it will no longer be needed after this call.
  utf16_text_ = nullptr;

  pac_url_ = GURL(pac_url);

  if (pac_url_.is_empty()) {
    std::move(callback_).Run(net::ERR_PAC_NOT_IN_DHCP);
    return;
  }

  int result = pac_file_fetcher_->Fetch(
      pac_url_, utf16_text,
      base::BindOnce(&DhcpPacFileFetcherMojo::OnFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      net::NetworkTrafficAnnotationTag(traffic_annotation_));
  if (result != net::ERR_IO_PENDING)
    std::move(callback_).Run(result);
}

void DhcpPacFileFetcherMojo::OnFetchCompleted(int result) {
  std::move(callback_).Run(result);
}

}  // namespace network
