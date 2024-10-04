// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/merkle_integrity_source_stream.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/browser/web_package/signed_exchange_certificate_chain.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_prologue.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/filter/source_stream.h"
#include "net/ssl/ssl_info.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/web_package/web_package_request_matcher.h"

namespace content {

namespace {

constexpr char kDigestHeader[] = "digest";
constexpr char kHistogramSignatureVerificationResult[] =
    "SignedExchange.SignatureVerificationResult";
constexpr char kHistogramCertVerificationResult[] =
    "SignedExchange.CertVerificationResult";
constexpr char kHistogramCTVerificationResult[] =
    "SignedExchange.CTVerificationResult";
constexpr char kHistogramOCSPResponseStatus[] =
    "SignedExchange.OCSPResponseStatus";
constexpr char kHistogramOCSPRevocationStatus[] =
    "SignedExchange.OCSPRevocationStatus";
constexpr char kSXGFromNonHTTPSErrorMessage[] =
    "Signed exchange response from non secure origin is not supported.";
constexpr char kSXGWithoutNoSniffErrorMessage[] =
    "Signed exchange response without \"X-Content-Type-Options: nosniff\" "
    "header is not supported.";

network::mojom::NetworkContext* g_network_context_for_testing = nullptr;
bool g_should_ignore_cert_validity_period_error = false;

bool IsSupportedSignedExchangeVersion(
    const std::optional<SignedExchangeVersion>& version) {
  return version == SignedExchangeVersion::kB3;
}

using VerifyCallback =
    base::OnceCallback<void(int32_t, const net::CertVerifyResult&, bool)>;

void VerifyCert(const scoped_refptr<net::X509Certificate>& certificate,
                const GURL& url,
                const std::string& ocsp_result,
                const std::string& sct_list,
                FrameTreeNodeId frame_tree_node_id,
                VerifyCallback callback) {
  VerifyCallback wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), net::ERR_FAILED, net::CertVerifyResult(), false);

  network::mojom::NetworkContext* network_context =
      g_network_context_for_testing;
  if (!network_context) {
    auto* frame = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    if (!frame)
      return;

    network_context = frame->current_frame_host()
                          ->GetProcess()
                          ->GetStoragePartition()
                          ->GetNetworkContext();
  }

  network_context->VerifyCertForSignedExchange(
      certificate, url, ocsp_result, sct_list, std::move(wrapped_callback));
}

std::string OCSPErrorToString(const bssl::OCSPVerifyResult& ocsp_result) {
  switch (ocsp_result.response_status) {
    case bssl::OCSPVerifyResult::PROVIDED:
      break;
    case bssl::OCSPVerifyResult::NOT_CHECKED:
      // This happens only in tests.
      return "OCSP verification was not performed.";
    case bssl::OCSPVerifyResult::MISSING:
      return "No OCSP Response was stapled.";
    case bssl::OCSPVerifyResult::ERROR_RESPONSE:
      return "OCSP response did not have a SUCCESSFUL status.";
    case bssl::OCSPVerifyResult::BAD_PRODUCED_AT:
      return "OCSP Response was produced at outside the certificate "
             "validity period.";
    case bssl::OCSPVerifyResult::NO_MATCHING_RESPONSE:
      return "OCSP Response did not match the certificate.";
    case bssl::OCSPVerifyResult::INVALID_DATE:
      return "OCSP Response was expired or not yet valid.";
    case bssl::OCSPVerifyResult::PARSE_RESPONSE_ERROR:
      return "OCSPResponse structure could not be parsed.";
    case bssl::OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR:
      return "OCSP ResponseData structure could not be parsed.";
    case bssl::OCSPVerifyResult::UNHANDLED_CRITICAL_EXTENSION:
      return "OCSP Response contained unhandled critical extension.";
  }

  switch (ocsp_result.revocation_status) {
    case bssl::OCSPRevocationStatus::GOOD:
      NOTREACHED_IN_MIGRATION();
      break;
    case bssl::OCSPRevocationStatus::REVOKED:
      return "OCSP response indicates that the certificate is revoked.";
    case bssl::OCSPRevocationStatus::UNKNOWN:
      return "OCSP responder doesn't know about the certificate.";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace

// static
void SignedExchangeHandler::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  g_network_context_for_testing = network_context;
}

// static
void SignedExchangeHandler::SetShouldIgnoreCertValidityPeriodErrorForTesting(
    bool ignore) {
  g_should_ignore_cert_validity_period_error = ignore;
}

SignedExchangeHandler::SignedExchangeHandler(
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
    FrameTreeNodeId frame_tree_node_id)
    : is_secure_transport_(is_secure_transport),
      has_nosniff_(has_nosniff),
      headers_callback_(std::move(headers_callback)),
      source_(std::move(body)),
      cert_fetcher_factory_(std::move(cert_fetcher_factory)),
      devtools_proxy_(std::move(devtools_proxy)),
      outer_request_isolation_info_(std::move(outer_request_isolation_info)),
      load_flags_(load_flags),
      remote_endpoint_(remote_endpoint),
      request_matcher_(std::move(request_matcher)),
      reporter_(reporter),
      frame_tree_node_id_(frame_tree_node_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::SignedExchangeHandler");

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#privacy-considerations
  // This can be difficult to determine when the exchange is being loaded from
  // local disk, but when the client itself requested the exchange over a
  // network it SHOULD require TLS ([I-D.ietf-tls-tls13]) or a successor
  // transport layer, and MUST NOT accept exchanges transferred over plain HTTP
  // without TLS. [spec text]
  if (!is_secure_transport_) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), kSXGFromNonHTTPSErrorMessage);
    // Proceed to extract and redirect to the fallback URL.
  }

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#seccons-content-sniffing
  // To encourage servers to include the `X-Content-Type-Options: nosniff`
  // header field, clients SHOULD reject signed exchanges served without it.
  // [spec text]
  if (!has_nosniff_) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), kSXGWithoutNoSniffErrorMessage);
    // Proceed to extract and redirect to the fallback URL.
  }

  version_ = signed_exchange_utils::GetSignedExchangeVersion(content_type);
  if (!IsSupportedSignedExchangeVersion(version_)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        base::StringPrintf("Unsupported version of the content type. Currently "
                           "content type must be "
                           "\"application/signed-exchange;v=b3\". But the "
                           "response content type was \"%s\"",
                           std::string(content_type).c_str()));
    // Proceed to extract and redirect to the fallback URL.
  }

  // Triggering the read (asynchronously) for the prologue bytes.
  SetupBuffers(
      signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                weak_factory_.GetWeakPtr()));
}

SignedExchangeHandler::~SignedExchangeHandler() = default;

SignedExchangeHandler::SignedExchangeHandler()
    : is_secure_transport_(true),
      has_nosniff_(true),
      load_flags_(net::LOAD_NORMAL) {}

const GURL& SignedExchangeHandler::GetFallbackUrl() const {
  return prologue_fallback_url_and_after_.fallback_url().url;
}

void SignedExchangeHandler::SetupBuffers(size_t size) {
  header_buf_ = base::MakeRefCounted<net::IOBufferWithSize>(size);
  header_read_buf_ =
      base::MakeRefCounted<net::DrainableIOBuffer>(header_buf_.get(), size);
}

void SignedExchangeHandler::DoHeaderLoop() {
  DCHECK(state_ == State::kReadingPrologueBeforeFallbackUrl ||
         state_ == State::kReadingPrologueFallbackUrlAndAfter ||
         state_ == State::kReadingHeaders);
  int rv =
      source_->Read(header_read_buf_.get(), header_read_buf_->BytesRemaining(),
                    base::BindOnce(&SignedExchangeHandler::DidReadHeader,
                                   base::Unretained(this), false /* sync */));
  if (rv != net::ERR_IO_PENDING)
    DidReadHeader(true /* sync */, rv);
}

void SignedExchangeHandler::DidReadHeader(bool completed_syncly,
                                          int read_result) {
  DCHECK(state_ == State::kReadingPrologueBeforeFallbackUrl ||
         state_ == State::kReadingPrologueFallbackUrlAndAfter ||
         state_ == State::kReadingHeaders);

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::DidReadHeader");
  if (read_result < 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        base::StringPrintf("Error reading body stream. result: %d",
                           read_result));
    RunErrorCallback(SignedExchangeLoadResult::kSXGHeaderNetError,
                     static_cast<net::Error>(read_result));
    return;
  }

  if (read_result == 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "Stream ended while reading signed exchange header.");
    SignedExchangeLoadResult result =
        GetFallbackUrl().is_valid()
            ? SignedExchangeLoadResult::kHeaderParseError
            : SignedExchangeLoadResult::kFallbackURLParseError;
    RunErrorCallback(result, net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  header_read_buf_->DidConsume(read_result);
  exchange_header_length_ += read_result;
  if (header_read_buf_->BytesRemaining() == 0) {
    SignedExchangeLoadResult result = SignedExchangeLoadResult::kSuccess;
    switch (state_) {
      case State::kReadingPrologueBeforeFallbackUrl:
        result = ParsePrologueBeforeFallbackUrl();
        break;
      case State::kReadingPrologueFallbackUrlAndAfter:
        result = ParsePrologueFallbackUrlAndAfter();
        break;
      case State::kReadingHeaders:
        result = ParseHeadersAndFetchCertificate();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    if (result != SignedExchangeLoadResult::kSuccess) {
      RunErrorCallback(result, net::ERR_INVALID_SIGNED_EXCHANGE);
      return;
    }
  }

  // We have finished reading headers, so return without queueing the next read.
  if (state_ == State::kFetchingCertificate)
    return;

  // Trigger the next read.
  DCHECK(state_ == State::kReadingPrologueBeforeFallbackUrl ||
         state_ == State::kReadingPrologueFallbackUrlAndAfter ||
         state_ == State::kReadingHeaders);
  if (completed_syncly) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SignedExchangeHandler::DoHeaderLoop,
                                  weak_factory_.GetWeakPtr()));
  } else {
    DoHeaderLoop();
  }
}

SignedExchangeLoadResult
SignedExchangeHandler::ParsePrologueBeforeFallbackUrl() {
  DCHECK_EQ(state_, State::kReadingPrologueBeforeFallbackUrl);

  prologue_before_fallback_url_ =
      signed_exchange_prologue::BeforeFallbackUrl::Parse(
          base::make_span(
              header_buf_->bytes(),
              signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes),
          devtools_proxy_.get());

  // Note: We will proceed even if |!prologue_before_fallback_url_.is_valid()|
  //       to attempt reading `fallbackUrl`.

  // Set up a new buffer for reading
  // |signed_exchange_prologue::FallbackUrlAndAfter|.
  SetupBuffers(
      prologue_before_fallback_url_.ComputeFallbackUrlAndAfterLength());
  state_ = State::kReadingPrologueFallbackUrlAndAfter;
  return SignedExchangeLoadResult::kSuccess;
}

SignedExchangeLoadResult
SignedExchangeHandler::ParsePrologueFallbackUrlAndAfter() {
  DCHECK_EQ(state_, State::kReadingPrologueFallbackUrlAndAfter);

  prologue_fallback_url_and_after_ =
      signed_exchange_prologue::FallbackUrlAndAfter::Parse(
          base::make_span(
              header_buf_->bytes(),
              prologue_before_fallback_url_.ComputeFallbackUrlAndAfterLength()),
          prologue_before_fallback_url_, devtools_proxy_.get());

  if (!GetFallbackUrl().is_valid())
    return SignedExchangeLoadResult::kFallbackURLParseError;

  if (!is_secure_transport_)
    return SignedExchangeLoadResult::kSXGServedFromNonHTTPS;

  if (!has_nosniff_)
    return SignedExchangeLoadResult::kSXGServedWithoutNosniff;

  // If the signed exchange version from content-type is unsupported or the
  // prologue's magic string is incorrect, abort parsing and redirect to the
  // fallback URL.
  if (!IsSupportedSignedExchangeVersion(version_) ||
      !prologue_before_fallback_url_.is_valid())
    return SignedExchangeLoadResult::kVersionMismatch;

  if (!prologue_fallback_url_and_after_.is_valid())
    return SignedExchangeLoadResult::kHeaderParseError;

  // Set up a new buffer for reading the Signature header field and CBOR-encoded
  // headers.
  SetupBuffers(
      prologue_fallback_url_and_after_.ComputeFollowingSectionsLength());
  state_ = State::kReadingHeaders;
  return SignedExchangeLoadResult::kSuccess;
}

SignedExchangeLoadResult
SignedExchangeHandler::ParseHeadersAndFetchCertificate() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::ParseHeadersAndFetchCertificate");
  DCHECK_EQ(state_, State::kReadingHeaders);

  DCHECK(version_.has_value());

  std::string_view data(header_buf_->data(), header_read_buf_->size());
  std::string_view signature_header_field = data.substr(
      0, prologue_fallback_url_and_after_.signature_header_field_length());
  base::span<const uint8_t> cbor_header =
      base::as_bytes(base::make_span(data.substr(
          prologue_fallback_url_and_after_.signature_header_field_length(),
          prologue_fallback_url_and_after_.cbor_header_length())));
  envelope_ = SignedExchangeEnvelope::Parse(
      *version_, prologue_fallback_url_and_after_.fallback_url(),
      signature_header_field, cbor_header, devtools_proxy_.get());
  header_read_buf_ = nullptr;
  header_buf_ = nullptr;
  if (!envelope_) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Failed to parse SignedExchange header.");
    return SignedExchangeLoadResult::kHeaderParseError;
  }

  if (reporter_) {
    reporter_->set_inner_url(envelope_->request_url().url);
    reporter_->set_cert_url(envelope_->signature().cert_url);
  }

  const GURL cert_url = envelope_->signature().cert_url;
  // TODO(crbug.com/40565993): When we will support ed25519Key, |cert_url|
  // may be empty.
  DCHECK(cert_url.is_valid());

  DCHECK(cert_fetcher_factory_);

  const bool force_fetch = load_flags_ & net::LOAD_BYPASS_CACHE;

  cert_fetch_start_time_ = base::TimeTicks::Now();
  cert_fetcher_ = std::move(cert_fetcher_factory_)
                      ->CreateFetcherAndStart(
                          cert_url, force_fetch,
                          base::BindOnce(&SignedExchangeHandler::OnCertReceived,
                                         base::Unretained(this)),
                          devtools_proxy_.get());

  state_ = State::kFetchingCertificate;
  return SignedExchangeLoadResult::kSuccess;
}

void SignedExchangeHandler::RunErrorCallback(SignedExchangeLoadResult result,
                                             net::Error error) {
  DCHECK_NE(state_, State::kHeadersCallbackCalled);
  if (devtools_proxy_) {
    devtools_proxy_->OnSignedExchangeReceived(
        envelope_,
        unverified_cert_chain_ ? unverified_cert_chain_->cert()
                               : scoped_refptr<net::X509Certificate>(),
        std::nullopt);
  }
  std::move(headers_callback_)
      .Run(result, error, GetFallbackUrl(), nullptr, nullptr);
  state_ = State::kHeadersCallbackCalled;
}

void SignedExchangeHandler::OnCertReceived(
    SignedExchangeLoadResult result,
    std::unique_ptr<SignedExchangeCertificateChain> cert_chain,
    net::IPAddress cert_server_ip_address) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::OnCertReceived");
  DCHECK_EQ(state_, State::kFetchingCertificate);

  base::TimeDelta cert_fetch_duration =
      base::TimeTicks::Now() - cert_fetch_start_time_;
  cert_server_ip_address_ = cert_server_ip_address;
  if (reporter_)
    reporter_->set_cert_server_ip_address(cert_server_ip_address_);

  if (result != SignedExchangeLoadResult::kSuccess) {
    UMA_HISTOGRAM_MEDIUM_TIMES("SignedExchange.Time.CertificateFetch.Failure",
                               cert_fetch_duration);

    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Failed to fetch the certificate.",
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    RunErrorCallback(result, net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("SignedExchange.Time.CertificateFetch.Success",
                             cert_fetch_duration);
  unverified_cert_chain_ = std::move(cert_chain);

  DCHECK(version_.has_value());
  const SignedExchangeSignatureVerifier::Result verify_result =
      SignedExchangeSignatureVerifier::Verify(
          *version_, *envelope_, unverified_cert_chain_.get(),
          signed_exchange_utils::GetVerificationTime(), devtools_proxy_.get());
  UMA_HISTOGRAM_ENUMERATION(kHistogramSignatureVerificationResult,
                            verify_result);
  if (verify_result != SignedExchangeSignatureVerifier::Result::kSuccess) {
    std::optional<SignedExchangeError::Field> error_field =
        SignedExchangeError::GetFieldFromSignatureVerifierResult(verify_result);
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Failed to verify the signed exchange header.",
        error_field ? std::make_optional(
                          std::make_pair(0 /* signature_index */, *error_field))
                    : std::nullopt);
    RunErrorCallback(
        signed_exchange_utils::GetLoadResultFromSignatureVerifierResult(
            verify_result),
        net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  auto certificate = unverified_cert_chain_->cert();
  auto url = envelope_->request_url().url;

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // Step 6.4 Validate that valid SCTs from trusted logs are available from any
  // of:
  // - The SignedCertificateTimestampList in main-certificate’s sct property
  //   (Section 3.3),
  const std::string& sct_list_from_cert_cbor = unverified_cert_chain_->sct();
  // - An OCSP extension in the OCSP response in main-certificate’s ocsp
  //   property, or
  const std::string& stapled_ocsp_response = unverified_cert_chain_->ocsp();

  VerifyCert(certificate, url, stapled_ocsp_response, sct_list_from_cert_cbor,
             frame_tree_node_id_,
             base::BindOnce(&SignedExchangeHandler::OnVerifyCert,
                            weak_factory_.GetWeakPtr()));
}

// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-cert-req
SignedExchangeLoadResult SignedExchangeHandler::CheckCertRequirements(
    const net::X509Certificate* verified_cert) {
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // Step 6.2. Validate that main-certificate has the CanSignHttpExchanges
  // extension (Section 4.2). [spec text]
  if (!net::asn1::HasCanSignHttpExchangesDraftExtension(
          net::x509_util::CryptoBufferAsStringPiece(
              verified_cert->cert_buffer())) &&
      !unverified_cert_chain_->ShouldIgnoreErrors()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "Certificate must have CanSignHttpExchangesDraft extension.",
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    return SignedExchangeLoadResult::kCertRequirementsNotMet;
  }

  // - After 2019-08-01, clients MUST reject all certificates with this
  // extension that have a Validity Period longer than 90 days. [spec text]
  base::TimeDelta validity_period =
      verified_cert->valid_expiry() - verified_cert->valid_start();
  if (validity_period > base::Days(90) &&
      !unverified_cert_chain_->ShouldIgnoreErrors() &&
      !g_should_ignore_cert_validity_period_error) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "After 2019-08-01, Signed Exchange's certificate must not have a "
        "validity period longer than 90 days.",
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    return SignedExchangeLoadResult::kCertValidityPeriodTooLong;
  }
  return SignedExchangeLoadResult::kSuccess;
}

bool SignedExchangeHandler::CheckOCSPStatus(
    const bssl::OCSPVerifyResult& ocsp_result) {
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // Step 6.3 Validate that main-certificate has an ocsp property (Section 3.3)
  // with a valid OCSP response whose lifetime (nextUpdate - thisUpdate) is less
  // than 7 days ([RFC6960]). [spec text]
  //
  // OCSP verification is done in CertVerifier::Verify(), so we just check the
  // result here.
  UMA_HISTOGRAM_ENUMERATION(kHistogramOCSPResponseStatus,
                            ocsp_result.response_status,
                            static_cast<base::HistogramBase::Sample>(
                                bssl::OCSPVerifyResult::RESPONSE_STATUS_MAX) +
                                1);
  if (ocsp_result.response_status == bssl::OCSPVerifyResult::PROVIDED) {
    UMA_HISTOGRAM_ENUMERATION(kHistogramOCSPRevocationStatus,
                              ocsp_result.revocation_status,
                              static_cast<base::HistogramBase::Sample>(
                                  bssl::OCSPRevocationStatus::MAX_VALUE) +
                                  1);
    if (ocsp_result.revocation_status == bssl::OCSPRevocationStatus::GOOD) {
      return true;
    }
  }
  return false;
}

void SignedExchangeHandler::OnVerifyCert(int32_t error_code,
                                         const net::CertVerifyResult& cv_result,
                                         bool pkp_bypassed) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeHandler::OnCertVerifyComplete");
  // net::Error codes are negative, so we put - in front of it.
  base::UmaHistogramSparse(kHistogramCertVerificationResult, -error_code);
  UMA_HISTOGRAM_ENUMERATION(kHistogramCTVerificationResult,
                            cv_result.policy_compliance,
                            net::ct::CTPolicyCompliance::CT_POLICY_COUNT);

  if (error_code != net::OK) {
    SignedExchangeLoadResult result;
    std::string error_message;
    if (error_code == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
      error_message = base::StringPrintf(
          "CT verification failed. result: %s, policy compliance: %d",
          net::ErrorToShortString(error_code).c_str(),
          static_cast<int>(cv_result.policy_compliance));
      result = SignedExchangeLoadResult::kCTVerificationError;
    } else {
      error_message =
          base::StringPrintf("Certificate verification error: %s",
                             net::ErrorToShortString(error_code).c_str());
      if (error_code == net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN)
        result = SignedExchangeLoadResult::kPKPViolationError;
      else
        result = SignedExchangeLoadResult::kCertVerificationError;
    }
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), error_message,
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    RunErrorCallback(result, net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  SignedExchangeLoadResult result =
      CheckCertRequirements(cv_result.verified_cert.get());
  if (result != SignedExchangeLoadResult::kSuccess) {
    RunErrorCallback(result, net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  if (!CheckOCSPStatus(cv_result.ocsp_result)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        base::StringPrintf("OCSP check failed: %s",
                           OCSPErrorToString(cv_result.ocsp_result).c_str()),
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureCertUrl));
    RunErrorCallback(SignedExchangeLoadResult::kOCSPError,
                     net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->is_signed_exchange_inner_response = true;

  response_head->headers = envelope_->BuildHttpResponseHeaders();
  response_head->headers->GetMimeTypeAndCharset(&response_head->mime_type,
                                                &response_head->charset);

  net::SSLInfo ssl_info;
  ssl_info.cert = cv_result.verified_cert;
  ssl_info.unverified_cert = unverified_cert_chain_->cert();
  ssl_info.cert_status = cv_result.cert_status;
  ssl_info.is_issued_by_known_root = cv_result.is_issued_by_known_root;
  ssl_info.pkp_bypassed = pkp_bypassed;
  ssl_info.public_key_hashes = cv_result.public_key_hashes;
  ssl_info.ocsp_result = cv_result.ocsp_result;
  ssl_info.is_fatal_cert_error = net::IsCertStatusError(ssl_info.cert_status);
  ssl_info.signed_certificate_timestamps = cv_result.scts;
  ssl_info.ct_policy_compliance = cv_result.policy_compliance;
  response_head->ssl_info = std::move(ssl_info);

  if (!request_matcher_->MatchRequest(envelope_->response_headers())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "Signed Exchange's Variants / Variant-Key don't match the request.");
    RunErrorCallback(SignedExchangeLoadResult::kVariantMismatch,
                     net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  // For prefetch, cookies will be checked by PrefetchedSignedExchangeCache.
  if (!(load_flags_ & net::LOAD_PREFETCH) &&
      signed_exchange_utils::IsCookielessOnlyExchange(
          *response_head->headers)) {
    CheckAbsenceOfCookies(base::BindOnce(&SignedExchangeHandler::CreateResponse,
                                         weak_factory_.GetWeakPtr(),
                                         std::move(response_head)));
    return;
  }
  CreateResponse(std::move(response_head));
}

void SignedExchangeHandler::CheckAbsenceOfCookies(base::OnceClosure callback) {
  auto* frame = FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!frame) {
    std::move(callback).Run();
    return;
  }
  CHECK(outer_request_isolation_info_.has_value());

  StoragePartition* storage_partition =
      frame->current_frame_host()->GetProcess()->GetStoragePartition();
  url::Origin inner_url_origin =
      url::Origin::Create(envelope_->request_url().url);
  net::IsolationInfo isolation_info =
      outer_request_isolation_info_->CreateForRedirect(inner_url_origin);

  RenderFrameHostImpl* render_frame_host = frame->current_frame_host();
  static_cast<StoragePartitionImpl*>(storage_partition)
      ->CreateRestrictedCookieManager(
          network::mojom::RestrictedCookieManagerRole::NETWORK,
          inner_url_origin, isolation_info,
          /* is_service_worker = */ false,
          render_frame_host ? render_frame_host->GetProcess()->GetID() : -1,
          render_frame_host ? render_frame_host->GetRoutingID()
                            : MSG_ROUTING_NONE,
          render_frame_host ? render_frame_host->GetCookieSettingOverrides()
                            : net::CookieSettingOverrides(),
          cookie_manager_.BindNewPipeAndPassReceiver(),
          render_frame_host ? render_frame_host->CreateCookieAccessObserver()
                            : mojo::NullRemote());

  CHECK(isolation_info.top_frame_origin().has_value());
  auto match_options = network::mojom::CookieManagerGetOptions::New();
  match_options->name = "";
  match_options->match_type = network::mojom::CookieMatchType::STARTS_WITH;
  // We set `storage_access_api_status` to kAccessViaAPI below in order to use
  // any Storage Access grant that exists, since a frame might go through this
  // code path and then obtain storage access in order to access cookies. Using
  // true instead of false here means we are being conservative, since this runs
  // an error callback if any cookies were retrieved.
  cookie_manager_->GetAllForUrl(
      envelope_->request_url().url, isolation_info.site_for_cookies(),
      *isolation_info.top_frame_origin(),
      net::StorageAccessApiStatus::kAccessViaAPI, std::move(match_options),
      /*is_ad_tagged=*/false,
      /*force_disable_third_party_cookies=*/false,
      base::BindOnce(&SignedExchangeHandler::OnGetCookies,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SignedExchangeHandler::OnGetCookies(
    base::OnceClosure callback,
    const std::vector<net::CookieWithAccessResult>& results) {
  if (!results.empty()) {
    RunErrorCallback(SignedExchangeLoadResult::kHadCookieForCookielessOnlySXG,
                     net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }
  std::move(callback).Run();
}

void SignedExchangeHandler::CreateResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  // TODO(crbug.com/40558902): Resource timing for signed exchange
  // loading is not speced yet. https://github.com/WICG/webpackage/issues/156
  response_head->load_timing.request_start_time = base::Time::Now();
  base::TimeTicks now(base::TimeTicks::Now());
  response_head->load_timing.request_start = now;
  response_head->load_timing.send_start = now;
  response_head->load_timing.send_end = now;
  response_head->load_timing.receive_headers_end = now;
  response_head->content_length = response_head->headers->GetContentLength();
  response_head->remote_endpoint = remote_endpoint_;

  auto body_stream = CreateResponseBodyStream();
  if (!body_stream) {
    RunErrorCallback(SignedExchangeLoadResult::kInvalidIntegrityHeader,
                     net::ERR_INVALID_SIGNED_EXCHANGE);
    return;
  }

  if (devtools_proxy_) {
    devtools_proxy_->OnSignedExchangeReceived(
        envelope_, unverified_cert_chain_->cert(), response_head->ssl_info);
  }

  std::move(headers_callback_)
      .Run(SignedExchangeLoadResult::kSuccess, net::OK,
           envelope_->request_url().url, std::move(response_head),
           std::move(body_stream));
  state_ = State::kHeadersCallbackCalled;
}

// https://wicg.github.io/webpackage/loading.html#read-a-body
std::unique_ptr<net::SourceStream>
SignedExchangeHandler::CreateResponseBodyStream() {
  if (!base::EqualsCaseInsensitiveASCII(envelope_->signature().integrity,
                                        "digest/mi-sha256-03")) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "The current implemention only supports \"digest/mi-sha256-03\" "
        "integrity scheme.",
        std::make_pair(0 /* signature_index */,
                       SignedExchangeError::Field::kSignatureIintegrity));
    return nullptr;
  }
  const auto& headers = envelope_->response_headers();
  auto digest_iter = headers.find(kDigestHeader);
  if (digest_iter == headers.end()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(), "Signed exchange has no Digest: header");
    return nullptr;
  }

  // For now, we allow only mi-sha256-03 content encoding.
  // TODO(crbug.com/41442806): Handle other content codings, such as gzip and
  // br.
  auto content_encoding_iter = headers.find("content-encoding");
  if (content_encoding_iter == headers.end()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "Signed exchange has no Content-Encoding: header");
    return nullptr;
  }
  if (!base::EqualsCaseInsensitiveASCII(content_encoding_iter->second,
                                        "mi-sha256-03")) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy_.get(),
        "Exchange's Content-Encoding must be \"mi-sha256-03\".");
    return nullptr;
  }

  return std::make_unique<MerkleIntegritySourceStream>(digest_iter->second,
                                                       std::move(source_));
}

bool SignedExchangeHandler::GetSignedExchangeInfoForPrefetchCache(
    PrefetchedSignedExchangeCacheEntry& entry) const {
  if (!envelope_)
    return false;
  entry.SetHeaderIntegrity(std::make_unique<net::SHA256HashValue>(
      envelope_->ComputeHeaderIntegrity()));
  entry.SetSignatureExpireTime(base::Time::UnixEpoch() +
                               base::Seconds(envelope_->signature().expires));
  entry.SetCertUrl(envelope_->signature().cert_url);
  entry.SetCertServerIPAddress(cert_server_ip_address_);
  return true;
}

}  // namespace content
