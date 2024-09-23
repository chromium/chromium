// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"

#include <optional>
#include <utility>

#include "base/unguessable_token.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace content {

SignedExchangeCertFetcherFactory::~SignedExchangeCertFetcherFactory() = default;

class SignedExchangeCertFetcherFactoryImpl
    : public SignedExchangeCertFetcherFactory {
 public:
  SignedExchangeCertFetcherFactoryImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      const std::optional<base::UnguessableToken>& throttling_profile_id,
      net::IsolationInfo isolation_info,
      const std::optional<url::Origin>& initiator)
      : url_loader_factory_(std::move(url_loader_factory)),
        url_loader_throttles_getter_(std::move(url_loader_throttles_getter)),
        throttling_profile_id_(throttling_profile_id),
        isolation_info_(std::move(isolation_info)),
        initiator_(initiator) {}

  std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& cert_url,
      bool force_fetch,
      SignedExchangeCertFetcher::CertificateCallback callback,
      SignedExchangeDevToolsProxy* devtools_proxy) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  URLLoaderThrottlesGetter url_loader_throttles_getter_;
  const std::optional<base::UnguessableToken> throttling_profile_id_;
  const net::IsolationInfo isolation_info_;
  const std::optional<url::Origin> initiator_;
};

std::unique_ptr<SignedExchangeCertFetcher>
SignedExchangeCertFetcherFactoryImpl::CreateFetcherAndStart(
    const GURL& cert_url,
    bool force_fetch,
    SignedExchangeCertFetcher::CertificateCallback callback,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  DCHECK(url_loader_factory_);
  DCHECK(url_loader_throttles_getter_);
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      std::move(url_loader_throttles_getter_).Run();
  return SignedExchangeCertFetcher::CreateAndStart(
      std::move(url_loader_factory_), std::move(throttles), cert_url,
      force_fetch, std::move(callback), devtools_proxy, throttling_profile_id_,
      isolation_info_, initiator_);
}

// static
std::unique_ptr<SignedExchangeCertFetcherFactory>
SignedExchangeCertFetcherFactory::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    const std::optional<base::UnguessableToken>& throttling_profile_id,
    net::IsolationInfo isolation_info,
    const std::optional<url::Origin>& initiator) {
  return std::make_unique<SignedExchangeCertFetcherFactoryImpl>(
      std::move(url_loader_factory), std::move(url_loader_throttles_getter),
      throttling_profile_id, std::move(isolation_info), initiator);
}

}  // namespace content
