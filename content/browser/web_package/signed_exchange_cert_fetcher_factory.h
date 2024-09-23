// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_FACTORY_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher.h"
#include "content/common/content_export.h"
#include "net/base/isolation_info.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace url {
class Origin;
}  // namespace url

namespace content {

class SignedExchangeDevToolsProxy;
class SignedExchangeCertFetcher;

// An interface for creating SignedExchangeCertFetcher object.
class CONTENT_EXPORT SignedExchangeCertFetcherFactory {
 public:
  virtual ~SignedExchangeCertFetcherFactory();
  // Creates a SignedExchangeCertFetcher and starts fetching the certificate.
  // Can be called at most once.
  virtual std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& cert_url,
      bool force_fetch,
      SignedExchangeCertFetcher::CertificateCallback callback,
      SignedExchangeDevToolsProxy* devtools_proxy) = 0;

  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>()>;
  static std::unique_ptr<SignedExchangeCertFetcherFactory> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      const std::optional<base::UnguessableToken>& throttling_profile_id,
      net::IsolationInfo isolation_info,
      const std::optional<url::Origin>& initiator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERT_FETCHER_FACTORY_H_
