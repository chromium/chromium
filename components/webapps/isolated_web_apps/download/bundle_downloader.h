// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_DOWNLOAD_BUNDLE_DOWNLOADER_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_DOWNLOAD_BUNDLE_DOWNLOADER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace net {
struct PartialNetworkTrafficAnnotationTag;
}  // namespace net

namespace web_app {

class COMPONENT_EXPORT(ISOLATED_WEB_APPS) ScopedTempWebBundleFile {
 public:
  // Creates a ScopedTempWebBundleFile on a non-blocking thread.
  // The result might be null if something goes wrong during the operation.
  static void Create(
      base::OnceCallback<void(ScopedTempWebBundleFile)> callback);

  explicit ScopedTempWebBundleFile(
      std::unique_ptr<base::ScopedTempFile> file = nullptr);

  // `file_` is deleted on a non-blocking thread.
  ~ScopedTempWebBundleFile();

  ScopedTempWebBundleFile& operator=(const ScopedTempWebBundleFile&) = delete;
  ScopedTempWebBundleFile(const ScopedTempWebBundleFile&) = delete;

  ScopedTempWebBundleFile& operator=(ScopedTempWebBundleFile&&);
  ScopedTempWebBundleFile(ScopedTempWebBundleFile&&);

  explicit operator bool() const { return !!file_; }

  const base::ScopedTempFile* file() const { return file_.get(); }

  // Will CHECK() if `file_` is nullptr.
  const base::FilePath& path() const;

 private:
  std::unique_ptr<base::ScopedTempFile> file_;
};

// Helper class to download the Signed Web Bundle of an Isolated Web App.
class COMPONENT_EXPORT(ISOLATED_WEB_APPS) IsolatedWebAppDownloader {
 public:
  using DownloadCallback = base::OnceCallback<void(int32_t net_error)>;
  using PartialDownloadCallback =
      base::OnceCallback<void(std::optional<std::string> data)>;

  static std::unique_ptr<IsolatedWebAppDownloader> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::mojom::NetworkContext* network_context);
  // Creates a new instance of this class and starts the download process.
  static std::unique_ptr<IsolatedWebAppDownloader> CreateAndStartDownloading(
      GURL url,
      base::FilePath destination,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::mojom::NetworkContext* network_context,
      DownloadCallback download_callback);

  explicit IsolatedWebAppDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::mojom::NetworkContext* network_context);
  ~IsolatedWebAppDownloader();

  void DownloadSignedWebBundle(
      GURL url,
      base::FilePath destination,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      DownloadCallback download_callback);
  // Downloads leading min(bundle_size, 8 KiB) bytes to string
  void DownloadInitialBytes(
      GURL url,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      PartialDownloadCallback download_callback);

 private:
  void OnHostResolved(
      base::OnceCallback<void(network::mojom::IPAddressSpace)>
          next_step_callback,
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const net::AddressList& resolved_addresses,
      const net::HostResolverEndpointResults& alternative_endpoints);

  void DownloadSignedWebBundleWithAddressSpace(
      GURL url,
      base::FilePath destination,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      DownloadCallback download_callback,
      network::mojom::IPAddressSpace client_space);

  void DownloadInitialBytesWithAddressSpace(
      GURL url,
      net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
      PartialDownloadCallback download_callback,
      network::mojom::IPAddressSpace client_space);

  int32_t OnSignedWebBundleDownloaded(base::FilePath destination,
                                      base::FilePath actual_destination);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleHostResolver> host_resolver_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<IsolatedWebAppDownloader> weak_factory_{this};
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_DOWNLOAD_BUNDLE_DOWNLOADER_H_
