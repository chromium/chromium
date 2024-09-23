// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_INNER_RESPONSE_URL_LOADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_INNER_RESPONSE_URL_LOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/cross_origin_read_blocking_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace content {

// A URLLoader which returns the inner response of signed exchange.
class CONTENT_EXPORT SignedExchangeInnerResponseURLLoader
    : public network::mojom::URLLoader {
 public:
  SignedExchangeInnerResponseURLLoader(
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr inner_response,
      std::unique_ptr<const storage::BlobDataHandle> blob_data_handle,
      const network::URLLoaderCompletionStatus& completion_status,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      bool is_navigation_request,
      scoped_refptr<base::RefCountedData<network::orb::PerFactoryState>>
          orb_state);

  SignedExchangeInnerResponseURLLoader(
      const SignedExchangeInnerResponseURLLoader&) = delete;
  SignedExchangeInnerResponseURLLoader& operator=(
      const SignedExchangeInnerResponseURLLoader&) = delete;

  ~SignedExchangeInnerResponseURLLoader() override;

  // Helper that is also used by other `URLLoaderFactory` implementations under
  // `web_package`.
  static void UpdateRequestResponseStartTime(
      network::mojom::URLResponseHead* response_head);

 private:
  static std::optional<std::string> GetHeaderString(
      const network::mojom::URLResponseHead& response,
      const std::string& header_name);

  void OnCrossOriginReadBlockingCheckComplete(
      CrossOriginReadBlockingChecker::Result result);

  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void SendResponseBody();

  void BlobReaderComplete(net::Error result);

  static void CreateMojoBlobReader(
      base::WeakPtr<SignedExchangeInnerResponseURLLoader> loader,
      mojo::ScopedDataPipeProducerHandle pipe_producer_handle,
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle);

  static void BlobReaderCompleteOnIO(
      base::WeakPtr<SignedExchangeInnerResponseURLLoader> loader,
      net::Error result);

  network::mojom::URLResponseHeadPtr response_;
  std::unique_ptr<const storage::BlobDataHandle> blob_data_handle_;
  const network::URLLoaderCompletionStatus completion_status_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  // `orb_checker_` references `orb_state_` so it needs to be destroyed first
  // (and therefore `orb_checker_`'s field declaration has to appear last).
  scoped_refptr<base::RefCountedData<network::orb::PerFactoryState>> orb_state_;
  std::unique_ptr<CrossOriginReadBlockingChecker> orb_checker_;

  base::WeakPtrFactory<SignedExchangeInnerResponseURLLoader> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_INNER_RESPONSE_URL_LOADER_H_
