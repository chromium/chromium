// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_VALIDITY_PINGER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_VALIDITY_PINGER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace blink {
class ThrottlingURLLoader;
class URLLoaderThrottle;
}  // namespace blink

namespace content {

// Sends a ping to the given |validity_url|. Current implementation is
// primarily for measurement and does no actual validity check: it only
// sends a HEAD request to the URL, wait for the response and then calls
// the given |callback| when it's done, regardless of whether it was success
// or not.
class CONTENT_EXPORT SignedExchangeValidityPinger
    : public network::mojom::URLLoaderClient,
      public mojo::DataPipeDrainer::Client {
 public:
  static std::unique_ptr<SignedExchangeValidityPinger> CreateAndStart(
      const GURL& validity_url,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      const base::Optional<base::UnguessableToken>& throttling_profile_id,
      base::OnceClosure callback);

  ~SignedExchangeValidityPinger() override;

 private:
  explicit SignedExchangeValidityPinger(base::OnceClosure callback);
  void Start(
      const GURL& validity_url,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      const base::Optional<base::UnguessableToken>& throttling_profile_id);

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // mojo::DataPipeDrainer::Client overrides:
  // This just does nothing but keep reading.
  void OnDataAvailable(const void* data, size_t num_bytes) override {}
  void OnDataComplete() override {}

  base::TimeTicks start_time_ = base::TimeTicks::Now();

  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeValidityPinger);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_VALIDITY_PINGER_H_
