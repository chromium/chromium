// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/browser/web_package/signed_exchange_prologue.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class CertVerifyResult;
class DrainableIOBuffer;
class SourceStream;
struct OCSPVerifyResult;
}  // namespace net

namespace network {
struct ResourceResponseHead;
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace content {

class SignedExchangeCertFetcher;
class SignedExchangeCertFetcherFactory;
class SignedExchangeCertificateChain;
class SignedExchangeDevToolsProxy;

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
  // - |request_url| and |request_method| are the exchange's request's URL and
  //   method.
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
      const std::string& request_method,
      const network::ResourceResponseHead& resource_response,
      std::unique_ptr<net::SourceStream> payload_stream)>;

  static void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context);

  static void SetVerificationTimeForTesting(
      base::Optional<base::Time> verification_time_for_testing);

  // Once constructed |this| starts reading the |body| and parses the response
  // as a signed HTTP exchange. The response body of the exchange can be read
  // from |payload_stream| passed to |headers_callback|. |cert_fetcher_factory|
  // is used to create a SignedExchangeCertFetcher that fetches the certificate.
  SignedExchangeHandler(
      std::string content_type,
      std::unique_ptr<net::SourceStream> body,
      ExchangeHeadersCallback headers_callback,
      std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory,
      int load_flags,
      std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy,
      base::RepeatingCallback<int(void)> frame_tree_node_id_getter);
  ~SignedExchangeHandler();

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
      std::unique_ptr<SignedExchangeCertificateChain> cert_chain);
  bool CheckCertExtension(const net::X509Certificate* verified_cert);
  bool CheckOCSPStatus(const net::OCSPVerifyResult& ocsp_result);

  void OnVerifyCert(int32_t error_code,
                    const net::CertVerifyResult& cv_result,
                    const net::ct::CTVerifyResult& ct_result);

  ExchangeHeadersCallback headers_callback_;
  base::Optional<SignedExchangeVersion> version_;
  std::unique_ptr<net::SourceStream> source_;

  State state_ = State::kReadingPrologueBeforeFallbackUrl;
  // Buffer used for prologue and envelope reading.
  scoped_refptr<net::IOBuffer> header_buf_;
  // Wrapper around |header_buf_| to progressively read fixed-size data.
  scoped_refptr<net::DrainableIOBuffer> header_read_buf_;

  signed_exchange_prologue::BeforeFallbackUrl prologue_before_fallback_url_;
  signed_exchange_prologue::FallbackUrlAndAfter
      prologue_fallback_url_and_after_;
  base::Optional<SignedExchangeEnvelope> envelope_;

  std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory_;
  std::unique_ptr<SignedExchangeCertFetcher> cert_fetcher_;
  const int load_flags_;

  std::unique_ptr<SignedExchangeCertificateChain> unverified_cert_chain_;

  std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy_;

  base::RepeatingCallback<int(void)> frame_tree_node_id_getter_;

  base::TimeTicks cert_fetch_start_time_;

  base::WeakPtrFactory<SignedExchangeHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeHandler);
};

// Used only for testing.
class SignedExchangeHandlerFactory {
 public:
  virtual ~SignedExchangeHandlerFactory() {}

  virtual std::unique_ptr<SignedExchangeHandler> Create(
      std::unique_ptr<net::SourceStream> body,
      SignedExchangeHandler::ExchangeHeadersCallback headers_callback,
      std::unique_ptr<SignedExchangeCertFetcherFactory>
          cert_fetcher_factory) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HANDLER_H_
