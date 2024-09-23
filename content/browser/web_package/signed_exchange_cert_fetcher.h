// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/signed_exchange_certificate_chain.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {
class SharedURLLoaderFactory;
struct ResourceRequest;
}  // namespace network

namespace mojo {
class SimpleWatcher;
}  // namespace mojo

namespace blink {
class ThrottlingURLLoader;
class URLLoaderThrottle;
}  // namespace blink

namespace url {
class Origin;
}  // namespace url

namespace content {

class SignedExchangeDevToolsProxy;

class CONTENT_EXPORT SignedExchangeCertFetcher
    : public network::mojom::URLLoaderClient {
 public:
  using CertificateCallback =
      base::OnceCallback<void(SignedExchangeLoadResult,
                              std::unique_ptr<SignedExchangeCertificateChain>,
                              net::IPAddress cert_server_ip_address)>;

  // Starts fetching the certificate using a ThrottlingURLLoader created with
  // the |shared_url_loader_factory| and the |throttles|. The |callback| will
  // be called with the certificate if succeeded. Otherwise it will be called
  // with null. If the returned fetcher is destructed before the |callback| is
  // called, the request will be canceled and the |callback| will no be called.
  //
  // Using SignedExchangeCertFetcherFactory is preferred rather than directly
  // calling this.
  static std::unique_ptr<SignedExchangeCertFetcher> CreateAndStart(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      const GURL& cert_url,
      bool force_fetch,
      CertificateCallback callback,
      SignedExchangeDevToolsProxy* devtools_proxy,
      const std::optional<base::UnguessableToken>& throttling_profile_id,
      net::IsolationInfo isolation_info,
      const std::optional<url::Origin>& initiator);

  SignedExchangeCertFetcher(const SignedExchangeCertFetcher&) = delete;
  SignedExchangeCertFetcher& operator=(const SignedExchangeCertFetcher&) =
      delete;

  ~SignedExchangeCertFetcher() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SignedExchangeCertFetcherTest, MaxCertSize_Exceeds);
  FRIEND_TEST_ALL_PREFIXES(SignedExchangeCertFetcherTest, MaxCertSize_SameSize);
  FRIEND_TEST_ALL_PREFIXES(SignedExchangeCertFetcherTest,
                           MaxCertSize_MultipleChunked);
  FRIEND_TEST_ALL_PREFIXES(SignedExchangeCertFetcherTest,
                           MaxCertSize_ContentLengthCheck);

  static base::ScopedClosureRunner SetMaxCertSizeForTest(size_t max_cert_size);

  SignedExchangeCertFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      const GURL& cert_url,
      bool force_fetch,
      CertificateCallback callback,
      SignedExchangeDevToolsProxy* devtools_proxy,
      const std::optional<base::UnguessableToken>& throttling_profile_id,
      net::IsolationInfo isolation_info,
      const std::optional<url::Origin>& initiator);
  void Start();
  void Abort();
  void OnHandleReady(MojoResult result);
  void OnDataComplete();

  void MaybeNotifyCompletionToDevtools(
      const network::URLLoaderCompletionStatus& status);

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void OnDataURLRequest(const network::ResourceRequest& resource_request,
                        mojo::PendingReceiver<network::mojom::URLLoader>,
                        mojo::PendingRemote<network::mojom::URLLoaderClient>);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles_;
  std::unique_ptr<network::ResourceRequest> resource_request_;
  CertificateCallback callback_;

  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;
  mojo::ScopedDataPipeConsumerHandle body_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;
  std::string body_string_;

  // This is owned by SignedExchangeHandler which is the owner of |this|.
  raw_ptr<SignedExchangeDevToolsProxy> devtools_proxy_;
  bool has_notified_completion_to_devtools_ = false;
  std::optional<base::UnguessableToken> cert_request_id_;

  net::IPAddress cert_server_ip_address_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_H_
