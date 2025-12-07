// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/browser/web_package/signed_exchange_prologue.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
class WebPackageRequestMatcher;
}  // namespace blink

namespace net {
class CertVerifyResult;
class DrainableIOBuffer;
class SourceStream;
}  // namespace net

namespace bssl {
struct OCSPVerifyResult;
}  // namespace bssl

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace content {

class PrefetchedSignedExchangeCacheEntry;
class SignedExchangeCertFetcher;
class SignedExchangeCertFetcherFactory;
class SignedExchangeCertificateChain;
class SignedExchangeDevToolsProxy;
class SignedExchangeReporter;

// SignedExchangeHandler reads "application/signed-exchange" format from a
// net::SourceStream, parses and verifies the signed exchange, and reports
// the result asynchronously via SignedExchangeHandler::ExchangeHeadersCallback.
//
// Note that verifying a signed exchange requires an associated certificate
// chain. SignedExchangeHandler creates a SignedExchangeCertFetcher to
// fetch the certificate chain over the network, and verifies it with the
// net::CertVerifier.
class CONTENT_EXPORT SignedExchangeHandler {
 public:
  // Callback that will be called when SXG header is parsed and its signature is
  // validated against a verified certificate, or on failure.
  // On success:
  // - |result| is kSuccess.
  // - |request_url| is the exchange's request's URL.
  // - |resource_response| contains inner response's headers.
  // - The payload stream can be read from |payload_stream|.
  // On failure:
  // - |result| indicates the failure reason.
  // - |error| indicates the net::Error if |result| is kSXGHeaderNetError.
  // - |request_url| may refer to the fallback URL.
  using ExchangeHeadersCallback = base::OnceCallback<void(
      SignedExchangeLoadResult result,
      net::Error error,
      const GURL& request_url,
      network::mojom::URLResponseHeadPtr resource_response,
      std::unique_ptr<net::SourceStream> payload_stream)>;

  static void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context);
  static void SetShouldIgnoreCertValidityPeriodErrorForTesting(bool ignore);

  // Once constructed |this| starts reading the |body| and parses the response
  // as a signed HTTP exchange. The response body of the exchange can be read
  // from |payload_stream| passed to |headers_callback|. |cert_fetcher_factory|
  // is used to create a SignedExchangeCertFetcher that fetches the certificate.
  SignedExchangeHandler(
      bool is_secure_transport,
      bool has_nosniff,
      std::string_view content_type,
      std::unique_ptr<net::SourceStream> body,
      ExchangeHeadersCallback headers_callback,
      std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory,
      std::optional<net::IsolationInfo> outer_request_isolation_info,
      int load_flags,
      const net::IPEndPoint& remote_endpoint,
      std::unique_ptr<blink::WebPackageRequestMatcher> request_matcher,
      std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy,
      SignedExchangeReporter* reporter,
      FrameTreeNodeId frame_tree_node_id);

  SignedExchangeHandler(const SignedExchangeHandler&) = delete;
  SignedExchangeHandler& operator=(const SignedExchangeHandler&) = delete;

  virtual ~SignedExchangeHandler();

  int64_t GetExchangeHeaderLength() const { return exchange_header_length_; }

  // Called to get the following information about the loaded signed exchange:
  //   - Header integrity value
  //   - Signature expire time
  //   - Cert URL
  //   - Cert server IP address
  // If failed to parse the signed exchange, this method fails and returns
  // false. Otherwise, returns true.
  virtual bool GetSignedExchangeInfoForPrefetchCache(
      PrefetchedSignedExchangeCacheEntry& entry) const;

 protected:
  SignedExchangeHandler();

 private:
  enum class State {
    kReadingPrologueBeforeFallbackUrl,
    kReadingPrologueFallbackUrlAndAfter,
    kReadingHeaders,
    kFetchingCertificate,
    kHeadersCallbackCalled,
  };

  const GURL& GetFallbackUrl() const;

  void SetupBuffers(size_t size);
  void DoHeaderLoop();
  void DidReadHeader(bool completed_syncly, int result);
  SignedExchangeLoadResult ParsePrologueBeforeFallbackUrl();
  SignedExchangeLoadResult ParsePrologueFallbackUrlAndAfter();
  SignedExchangeLoadResult ParseHeadersAndFetchCertificate();
  void RunErrorCallback(SignedExchangeLoadResult result, net::Error);

  void OnCertReceived(
      SignedExchangeLoadResult result,
      std::unique_ptr<SignedExchangeCertificateChain> cert_chain,
      net::IPAddress cert_server_ip_address);
  SignedExchangeLoadResult CheckCertRequirements(
      const net::X509Certificate* verified_cert);
  bool CheckOCSPStatus(const bssl::OCSPVerifyResult& ocsp_result);

  void OnVerifyCert(int32_t error_code,
                    const net::CertVerifyResult& cv_result,
                    bool pkp_bypassed);
  void CheckAbsenceOfCookies(base::OnceClosure callback);
  void OnGetCookies(base::OnceClosure callback,
                    const std::vector<net::CookieWithAccessResult>& results);
  void CreateResponse(network::mojom::URLResponseHeadPtr response_head);
  std::unique_ptr<net::SourceStream> CreateResponseBodyStream();

  const bool is_secure_transport_;
  const bool has_nosniff_;
  ExchangeHeadersCallback headers_callback_;
  std::optional<SignedExchangeVersion> version_;
  std::unique_ptr<net::SourceStream> source_;

  State state_ = State::kReadingPrologueBeforeFallbackUrl;
  // Buffer used for prologue and envelope reading.
  scoped_refptr<net::IOBuffer> header_buf_;
  // Wrapper around |header_buf_| to progressively read fixed-size data.
  scoped_refptr<net::DrainableIOBuffer> header_read_buf_;
  int64_t exchange_header_length_ = 0;

  signed_exchange_prologue::BeforeFallbackUrl prologue_before_fallback_url_;
  signed_exchange_prologue::FallbackUrlAndAfter
      prologue_fallback_url_and_after_;
  std::optional<SignedExchangeEnvelope> envelope_;

  std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory_;

  std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy_;

  // `cert_fetcher_` borrows reference from `devtools_proxy_`, so it needs to be
  // declared last, so that it is destroyed first.
  std::unique_ptr<SignedExchangeCertFetcher> cert_fetcher_;
  // `outer_request_isolation_info_` will be set unless this corresponds to a
  // prefetch request.
  std::optional<net::IsolationInfo> outer_request_isolation_info_;
  const int load_flags_ = 0;
  const net::IPEndPoint remote_endpoint_;

  std::unique_ptr<SignedExchangeCertificateChain> unverified_cert_chain_;

  std::unique_ptr<blink::WebPackageRequestMatcher> request_matcher_;
  mojo::Remote<network::mojom::RestrictedCookieManager> cookie_manager_;

  // This is owned by SignedExchangeLoader which is the owner of |this|.
  raw_ptr<SignedExchangeReporter> reporter_;

  const FrameTreeNodeId frame_tree_node_id_;

  base::TimeTicks cert_fetch_start_time_;
  net::IPAddress cert_server_ip_address_;

  base::WeakPtrFactory<SignedExchangeHandler> weak_factory_{this};
};

// Used only for testing.
class SignedExchangeHandlerFactory {
 public:
  virtual ~SignedExchangeHandlerFactory() {}

  virtual std::unique_ptr<SignedExchangeHandler> Create(
      const GURL& outer_url,
      std::unique_ptr<net::SourceStream> body,
      SignedExchangeHandler::ExchangeHeadersCallback headers_callback,
      std::unique_ptr<SignedExchangeCertFetcherFactory>
          cert_fetcher_factory) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_
