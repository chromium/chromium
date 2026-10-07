// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/download/bundle_downloader.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

ScopedTempWebBundleFile CreateScopedTempFileBlocking() {
  auto file = std::make_unique<base::ScopedTempFile>();
  if (!file->Create()) {
    return ScopedTempWebBundleFile(/*file=*/nullptr);
  }
  return ScopedTempWebBundleFile(std::move(file));
}

}  // namespace

void ScopedTempWebBundleFile::Create(
    base::OnceCallback<void(ScopedTempWebBundleFile)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CreateScopedTempFileBlocking), std::move(callback));
}

ScopedTempWebBundleFile::ScopedTempWebBundleFile(
    std::unique_ptr<base::ScopedTempFile> file)
    : file_(std::move(file)) {}

ScopedTempWebBundleFile::~ScopedTempWebBundleFile() {
  if (!file_) {
    return;
  }

  // Deleting the file must happen on a thread that allows blocking.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](std::unique_ptr<base::ScopedTempFile> file) {
            // `file` is deleted here.
          },
          std::move(file_)));
}

ScopedTempWebBundleFile& ScopedTempWebBundleFile::operator=(
    ScopedTempWebBundleFile&&) = default;
ScopedTempWebBundleFile::ScopedTempWebBundleFile(ScopedTempWebBundleFile&&) =
    default;

const base::FilePath& ScopedTempWebBundleFile::path() const {
  CHECK(file_) << "`path()` must not be called on a nullptr `file_`.";
  return file_->path();
}

// static
std::unique_ptr<IsolatedWebAppDownloader> IsolatedWebAppDownloader::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::NetworkContext* network_context) {
  return base::WrapUnique(new IsolatedWebAppDownloader(
      std::move(url_loader_factory), network_context));
}

// static
std::unique_ptr<IsolatedWebAppDownloader>
IsolatedWebAppDownloader::CreateAndStartDownloading(
    GURL url,
    base::FilePath destination,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::NetworkContext* network_context,
    IsolatedWebAppDownloader::DownloadCallback download_callback) {
  auto downloader = Create(std::move(url_loader_factory), network_context);
  downloader->DownloadSignedWebBundle(std::move(url), std::move(destination),
                                      std::move(partial_traffic_annotation),
                                      std::move(download_callback));
  return downloader;
}

IsolatedWebAppDownloader::IsolatedWebAppDownloader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::NetworkContext* network_context)
    : url_loader_factory_(std::move(url_loader_factory)),
      host_resolver_(network::SimpleHostResolver::Create(network_context)) {}

IsolatedWebAppDownloader::~IsolatedWebAppDownloader() = default;

namespace {
net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag(
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation) {
  return net::CompleteNetworkTrafficAnnotation("iwa_bundle_downloader",
                                               partial_traffic_annotation,
                                               R"(
    semantics {
      data:
        "This request does not send any user data. Its destination is the URL "
        "of a Signed Web Bundle of an Isolated Web App that is installed for "
        "the user."
      destination: OTHER
      internal {
        contacts {
          owners: "//chrome/browser/web_applications/isolated_web_apps/OWNERS"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-06-01"
    }
    policy {
      cookies_allowed: NO
    })");
}
}  // namespace

void IsolatedWebAppDownloader::DownloadSignedWebBundle(
    GURL url,
    base::FilePath destination,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    DownloadCallback download_callback) {
  if (auto space = network::GetAddressSpaceFromUrl(url)) {
    DownloadSignedWebBundleWithAddressSpace(
        std::move(url), std::move(destination),
        std::move(partial_traffic_annotation), std::move(download_callback),
        *space);
    return;
  }

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->initial_priority = net::RequestPriority::HIGHEST;

  GURL url_copy = url;
  host_resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair::FromURL(url_copy)),
      net::NetworkAnonymizationKey(), std::move(parameters),
      base::BindOnce(&IsolatedWebAppDownloader::OnHostResolved,
                     weak_factory_.GetWeakPtr(),
                     base::BindOnce(&IsolatedWebAppDownloader::
                                        DownloadSignedWebBundleWithAddressSpace,
                                    weak_factory_.GetWeakPtr(), std::move(url),
                                    std::move(destination),
                                    std::move(partial_traffic_annotation),
                                    std::move(download_callback))));
}

void IsolatedWebAppDownloader::DownloadInitialBytes(
    GURL url,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    PartialDownloadCallback download_callback) {
  if (auto space = network::GetAddressSpaceFromUrl(url)) {
    DownloadInitialBytesWithAddressSpace(std::move(url),
                                         std::move(partial_traffic_annotation),
                                         std::move(download_callback), *space);
    return;
  }

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->initial_priority = net::RequestPriority::HIGHEST;

  GURL url_copy = url;
  host_resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair::FromURL(url_copy)),
      net::NetworkAnonymizationKey(), std::move(parameters),
      base::BindOnce(
          &IsolatedWebAppDownloader::OnHostResolved, weak_factory_.GetWeakPtr(),
          base::BindOnce(
              &IsolatedWebAppDownloader::DownloadInitialBytesWithAddressSpace,
              weak_factory_.GetWeakPtr(), std::move(url),
              std::move(partial_traffic_annotation),
              std::move(download_callback))));
}

void IsolatedWebAppDownloader::OnHostResolved(
    base::OnceCallback<void(network::mojom::IPAddressSpace)> next_step_callback,
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const net::AddressList& resolved_addresses,
    const net::HostResolverEndpointResults& alternative_endpoints) {
  network::mojom::IPAddressSpace space =
      network::mojom::IPAddressSpace::kUnknown;
  if (result == net::OK && !resolved_addresses.empty()) {
    // If resolved_addresses contains multiple addresses with different address
    // spaces (e.g. both public and private IPs), using
    // resolved_addresses.front() might lead to a security bypass if the network
    // service ends up connecting to a public IP but we set the client security
    // state to kLocal (based on the first address being private). To be safe,
    // we iterate over all resolved addresses and choose the 'most public'
    // (least privileged) address space (i.e. kPublic > kLocal > kLoopback).
    space = network::IPAddressToIPAddressSpace(
        std::ranges::max(resolved_addresses, network::IsLessPublicAddressSpace,
                         [](const auto& endpoint) {
                           return network::IPAddressToIPAddressSpace(
                               endpoint.address());
                         })
            .address());
  }
  std::move(next_step_callback).Run(space);
}

void IsolatedWebAppDownloader::DownloadSignedWebBundleWithAddressSpace(
    GURL url,
    base::FilePath destination,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    DownloadCallback download_callback,
    network::mojom::IPAddressSpace client_space) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetNetworkTrafficAnnotationTag(std::move(partial_traffic_annotation));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto client_security_state = network::mojom::ClientSecurityState::New();
  client_security_state->ip_address_space = client_space;
  client_security_state->is_web_secure_context = true;
  client_security_state->local_network_access_request_policy =
      network::mojom::LocalNetworkAccessRequestPolicy::kBlock;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->client_security_state =
      std::move(client_security_state);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), std::move(traffic_annotation));

  simple_url_loader_->SetRetryOptions(
      /* max_retries=*/3,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  simple_url_loader_->DownloadToFile(
      url_loader_factory_.get(),
      base::BindOnce(&IsolatedWebAppDownloader::OnSignedWebBundleDownloaded,
                     // The callback will never run if `this` is deleted,
                     // because `simple_url_loader_` is a member of `this`.
                     base::Unretained(this), destination)
          .Then(std::move(download_callback)),
      destination);
}

void IsolatedWebAppDownloader::DownloadInitialBytesWithAddressSpace(
    GURL url,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    PartialDownloadCallback download_callback,
    network::mojom::IPAddressSpace client_space) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetNetworkTrafficAnnotationTag(std::move(partial_traffic_annotation));
  // 8 KiB - this should be enough to contain the entire integrity block of any
  // signed web bundle. While it is technically possible to create a bigger one,
  // there's no reason to do that. In such case, if it happens, this whole
  // heuristic is skipped and the entire bundle is downloaded.
  constexpr size_t kMaxInitialBytes = 8 * 1024;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::Bounded(0, kMaxInitialBytes - 1).GetHeaderValue());

  auto client_security_state = network::mojom::ClientSecurityState::New();
  client_security_state->ip_address_space = client_space;
  client_security_state->is_web_secure_context = true;
  client_security_state->local_network_access_request_policy =
      network::mojom::LocalNetworkAccessRequestPolicy::kBlock;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->client_security_state =
      std::move(client_security_state);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), std::move(traffic_annotation));

  simple_url_loader_->SetRetryOptions(
      /* max_retries=*/3,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetAllowPartialResults(true);

  simple_url_loader_->DownloadToString(url_loader_factory_.get(),
                                       std::move(download_callback),
                                       kMaxInitialBytes);
}

int32_t IsolatedWebAppDownloader::OnSignedWebBundleDownloaded(
    base::FilePath destination,
    base::FilePath actual_destination) {
  if (actual_destination.empty()) {
    int32_t net_error = simple_url_loader_->NetError();
    CHECK_NE(net_error, net::OK);
    return net_error;
  }
  CHECK_EQ(actual_destination, destination);
  return net::OK;
}

}  // namespace web_app
