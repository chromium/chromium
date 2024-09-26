// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/test/embedded_test_server/embedded_test_server.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "crypto/rsa_private_key.h"
#include "net/base/hex_utils.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/log/net_log_source.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/key_util.h"
#include "net/test/revocation_builder.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_frame_builder.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "url/origin.h"

namespace net::test_server {

namespace {

std::unique_ptr<HttpResponse> ServeResponseForPath(
    const std::string& expected_path,
    HttpStatusCode status_code,
    const std::string& content_type,
    const std::string& content,
    const HttpRequest& request) {
  if (request.GetURL().path() != expected_path)
    return nullptr;

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(status_code);
  http_response->set_content_type(content_type);
  http_response->set_content(content);
  return http_response;
}

// Serves response for |expected_path| or any subpath of it.
// |expected_path| should not include a trailing "/".
std::unique_ptr<HttpResponse> ServeResponseForSubPaths(
    const std::string& expected_path,
    HttpStatusCode status_code,
    const std::string& content_type,
    const std::string& content,
    const HttpRequest& request) {
  if (request.GetURL().path() != expected_path &&
      !request.GetURL().path().starts_with(expected_path + "/")) {
    return nullptr;
  }

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(status_code);
  http_response->set_content_type(content_type);
  http_response->set_content(content);
  return http_response;
}

bool MaybeCreateOCSPResponse(CertBuilder* target,
                             const EmbeddedTestServer::OCSPConfig& config,
                             std::string* out_response) {
  using OCSPResponseType = EmbeddedTestServer::OCSPConfig::ResponseType;

  if (!config.single_responses.empty() &&
      config.response_type != OCSPResponseType::kSuccessful) {
    // OCSPConfig contained single_responses for a non-successful response.
    return false;
  }

  if (config.response_type == OCSPResponseType::kOff) {
    *out_response = std::string();
    return true;
  }

  if (!target) {
    // OCSPConfig enabled but corresponding certificate is null.
    return false;
  }

  switch (config.response_type) {
    case OCSPResponseType::kOff:
      return false;
    case OCSPResponseType::kMalformedRequest:
      *out_response = BuildOCSPResponseError(
          bssl::OCSPResponse::ResponseStatus::MALFORMED_REQUEST);
      return true;
    case OCSPResponseType::kInternalError:
      *out_response = BuildOCSPResponseError(
          bssl::OCSPResponse::ResponseStatus::INTERNAL_ERROR);
      return true;
    case OCSPResponseType::kTryLater:
      *out_response =
          BuildOCSPResponseError(bssl::OCSPResponse::ResponseStatus::TRY_LATER);
      return true;
    case OCSPResponseType::kSigRequired:
      *out_response = BuildOCSPResponseError(
          bssl::OCSPResponse::ResponseStatus::SIG_REQUIRED);
      return true;
    case OCSPResponseType::kUnauthorized:
      *out_response = BuildOCSPResponseError(
          bssl::OCSPResponse::ResponseStatus::UNAUTHORIZED);
      return true;
    case OCSPResponseType::kInvalidResponse:
      *out_response = "3";
      return true;
    case OCSPResponseType::kInvalidResponseData:
      *out_response =
          BuildOCSPResponseWithResponseData(target->issuer()->GetKey(),
                                            // OCTET_STRING { "not ocsp data" }
                                            "\x04\x0dnot ocsp data");
      return true;
    case OCSPResponseType::kSuccessful:
      break;
  }

  base::Time now = base::Time::Now();
  base::Time target_not_before, target_not_after;
  if (!target->GetValidity(&target_not_before, &target_not_after))
    return false;
  base::Time produced_at;
  using OCSPProduced = EmbeddedTestServer::OCSPConfig::Produced;
  switch (config.produced) {
    case OCSPProduced::kValid:
      produced_at = std::max(now - base::Days(1), target_not_before);
      break;
    case OCSPProduced::kBeforeCert:
      produced_at = target_not_before - base::Days(1);
      break;
    case OCSPProduced::kAfterCert:
      produced_at = target_not_after + base::Days(1);
      break;
  }

  std::vector<OCSPBuilderSingleResponse> responses;
  for (const auto& config_response : config.single_responses) {
    OCSPBuilderSingleResponse response;
    response.serial = target->GetSerialNumber();
    if (config_response.serial ==
        EmbeddedTestServer::OCSPConfig::SingleResponse::Serial::kMismatch) {
      response.serial ^= 1;
    }
    response.cert_status = config_response.cert_status;
    // |revocation_time| is ignored if |cert_status| is not REVOKED.
    response.revocation_time = now - base::Days(1000);

    using OCSPDate = EmbeddedTestServer::OCSPConfig::SingleResponse::Date;
    switch (config_response.ocsp_date) {
      case OCSPDate::kValid:
        response.this_update = now - base::Days(1);
        response.next_update = response.this_update + base::Days(7);
        break;
      case OCSPDate::kOld:
        response.this_update = now - base::Days(8);
        response.next_update = response.this_update + base::Days(7);
        break;
      case OCSPDate::kEarly:
        response.this_update = now + base::Days(1);
        response.next_update = response.this_update + base::Days(7);
        break;
      case OCSPDate::kLong:
        response.this_update = now - base::Days(365);
        response.next_update = response.this_update + base::Days(366);
        break;
      case OCSPDate::kLonger:
        response.this_update = now - base::Days(367);
        response.next_update = response.this_update + base::Days(368);
        break;
    }

    responses.push_back(response);
  }
  *out_response =
      BuildOCSPResponse(target->issuer()->GetSubject(),
                        target->issuer()->GetKey(), produced_at, responses);
  return true;
}

void DispatchResponseToDelegate(std::unique_ptr<HttpResponse> response,
                                base::WeakPtr<HttpResponseDelegate> delegate) {
  HttpResponse* const response_ptr = response.get();
  delegate->AddResponse(std::move(response));
  response_ptr->SendResponse(delegate);
}

}  // namespace

EmbeddedTestServerHandle::EmbeddedTestServerHandle(
    EmbeddedTestServerHandle&& other) {
  operator=(std::move(other));
}

EmbeddedTestServerHandle& EmbeddedTestServerHandle::operator=(
    EmbeddedTestServerHandle&& other) {
  EmbeddedTestServerHandle temporary;
  std::swap(other.test_server_, temporary.test_server_);
  std::swap(temporary.test_server_, test_server_);
  return *this;
}

EmbeddedTestServerHandle::EmbeddedTestServerHandle(
    EmbeddedTestServer* test_server)
    : test_server_(test_server) {}

EmbeddedTestServerHandle::~EmbeddedTestServerHandle() {
  if (test_server_)
    CHECK(test_server_->ShutdownAndWaitUntilComplete());
}

EmbeddedTestServer::OCSPConfig::OCSPConfig() = default;
EmbeddedTestServer::OCSPConfig::OCSPConfig(ResponseType response_type)
    : response_type(response_type) {}
EmbeddedTestServer::OCSPConfig::OCSPConfig(
    std::vector<SingleResponse> single_responses,
    Produced produced)
    : response_type(ResponseType::kSuccessful),
      produced(produced),
      single_responses(std::move(single_responses)) {}
EmbeddedTestServer::OCSPConfig::OCSPConfig(const OCSPConfig&) = default;
EmbeddedTestServer::OCSPConfig::OCSPConfig(OCSPConfig&&) = default;
EmbeddedTestServer::OCSPConfig::~OCSPConfig() = default;
EmbeddedTestServer::OCSPConfig& EmbeddedTestServer::OCSPConfig::operator=(
    const OCSPConfig&) = default;
EmbeddedTestServer::OCSPConfig& EmbeddedTestServer::OCSPConfig::operator=(
    OCSPConfig&&) = default;

EmbeddedTestServer::ServerCertificateConfig::ServerCertificateConfig() =
    default;
EmbeddedTestServer::ServerCertificateConfig::ServerCertificateConfig(
    const ServerCertificateConfig&) = default;
EmbeddedTestServer::ServerCertificateConfig::ServerCertificateConfig(
    ServerCertificateConfig&&) = default;
EmbeddedTestServer::ServerCertificateConfig::~ServerCertificateConfig() =
    default;
EmbeddedTestServer::ServerCertificateConfig&
EmbeddedTestServer::ServerCertificateConfig::operator=(
    const ServerCertificateConfig&) = default;
EmbeddedTestServer::ServerCertificateConfig&
EmbeddedTestServer::ServerCertificateConfig::operator=(
    ServerCertificateConfig&&) = default;

EmbeddedTestServer::EmbeddedTestServer() : EmbeddedTestServer(TYPE_HTTP) {}

EmbeddedTestServer::EmbeddedTestServer(Type type,
                                       HttpConnection::Protocol protocol)
    : is_using_ssl_(type == TYPE_HTTPS), protocol_(protocol) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // HTTP/2 is only valid by negotiation via TLS ALPN
  DCHECK(protocol_ != HttpConnection::Protocol::kHttp2 || type == TYPE_HTTPS);

  if (!is_using_ssl_)
    return;
  scoped_test_root_ = RegisterTestCerts();
}

EmbeddedTestServer::~EmbeddedTestServer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (Started())
    CHECK(ShutdownAndWaitUntilComplete());

  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_for_thread_join;
    io_thread_.reset();
  }
}

ScopedTestRoot EmbeddedTestServer::RegisterTestCerts() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto root = ImportCertFromFile(GetRootCertPemPath());
  if (!root)
    return ScopedTestRoot();
  return ScopedTestRoot(root);
}

void EmbeddedTestServer::SetConnectionListener(
    EmbeddedTestServerConnectionListener* listener) {
  DCHECK(!io_thread_)
      << "ConnectionListener must be set before starting the server.";
  connection_listener_ = listener;
}

EmbeddedTestServerHandle EmbeddedTestServer::StartAndReturnHandle(int port) {
  bool result = Start(port);
  return result ? EmbeddedTestServerHandle(this) : EmbeddedTestServerHandle();
}

bool EmbeddedTestServer::Start(int port, std::string_view address) {
  bool success = InitializeAndListen(port, address);
  if (success)
    StartAcceptingConnections();
  return success;
}

bool EmbeddedTestServer::InitializeAndListen(int port,
                                             std::string_view address) {
  DCHECK(!Started());

  const int max_tries = 5;
  int num_tries = 0;
  bool is_valid_port = false;

  do {
    if (++num_tries > max_tries) {
      LOG(ERROR) << "Failed to listen on a valid port after " << max_tries
                 << " attempts.";
      listen_socket_.reset();
      return false;
    }

    listen_socket_ = std::make_unique<TCPServerSocket>(nullptr, NetLogSource());

    int result =
        listen_socket_->ListenWithAddressAndPort(address.data(), port, 10);
    if (result) {
      LOG(ERROR) << "Listen failed: " << ErrorToString(result);
      listen_socket_.reset();
      return false;
    }

    result = listen_socket_->GetLocalAddress(&local_endpoint_);
    if (result != OK) {
      LOG(ERROR) << "GetLocalAddress failed: " << ErrorToString(result);
      listen_socket_.reset();
      return false;
    }

    port_ = local_endpoint_.port();
    is_valid_port |= net::IsPortAllowedForScheme(
        port_, is_using_ssl_ ? url::kHttpsScheme : url::kHttpScheme);
  } while (!is_valid_port);

  if (is_using_ssl_) {
    base_url_ = GURL("https://" + local_endpoint_.ToString());
    if (cert_ == CERT_MISMATCHED_NAME || cert_ == CERT_COMMON_NAME_IS_DOMAIN) {
      base_url_ = GURL(
          base::StringPrintf("https://localhost:%d", local_endpoint_.port()));
    }
  } else {
    base_url_ = GURL("http://" + local_endpoint_.ToString());
  }

  listen_socket_->DetachFromThread();

  if (is_using_ssl_ && !InitializeSSLServerContext())
    return false;

  return true;
}

bool EmbeddedTestServer::UsingStaticCert() const {
  return !GetCertificateName().empty();
}

bool EmbeddedTestServer::InitializeCertAndKeyFromFile() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath certs_dir(GetTestCertsDirectory());
  std::string cert_name = GetCertificateName();
  if (cert_name.empty())
    return false;

  x509_cert_ = CreateCertificateChainFromFile(certs_dir, cert_name,
                                              X509Certificate::FORMAT_AUTO);
  if (!x509_cert_)
    return false;

  private_key_ =
      key_util::LoadEVP_PKEYFromPEM(certs_dir.AppendASCII(cert_name));
  return !!private_key_;
}

bool EmbeddedTestServer::GenerateCertAndKey() {
  // Create AIA server and start listening. Need to have the socket initialized
  // so the URL can be put in the AIA records of the generated certs.
  aia_http_server_ = std::make_unique<EmbeddedTestServer>(TYPE_HTTP);
  if (!aia_http_server_->InitializeAndListen())
    return false;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath certs_dir(GetTestCertsDirectory());
  auto now = base::Time::Now();

  std::unique_ptr<CertBuilder> root;
  switch (cert_config_.root) {
    case RootType::kTestRootCa:
      root = CertBuilder::FromStaticCertFile(
          certs_dir.AppendASCII("root_ca_cert.pem"));
      break;
    case RootType::kUniqueRoot:
      root = std::make_unique<CertBuilder>(nullptr, nullptr);
      root->SetValidity(now - base::Days(100), now + base::Days(1000));
      root->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
      root->SetKeyUsages(
          {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});
      break;
  }

  // Will be nullptr if cert_config_.intermediate == kNone.
  std::unique_ptr<CertBuilder> intermediate;
  std::unique_ptr<CertBuilder> leaf;

  if (cert_config_.intermediate != IntermediateType::kNone) {
    intermediate = std::make_unique<CertBuilder>(nullptr, root.get());
    intermediate->SetValidity(now - base::Days(100), now + base::Days(1000));
    intermediate->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
    intermediate->SetKeyUsages(
        {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});

    leaf = std::make_unique<CertBuilder>(nullptr, intermediate.get());
  } else {
    leaf = std::make_unique<CertBuilder>(nullptr, root.get());
  }
  std::vector<GURL> leaf_ca_issuers_urls;
  std::vector<GURL> leaf_ocsp_urls;

  leaf->SetValidity(now - base::Days(1), now + base::Days(20));
  leaf->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);
  leaf->SetExtendedKeyUsages({bssl::der::Input(bssl::kServerAuth)});

  if (!cert_config_.policy_oids.empty()) {
    leaf->SetCertificatePolicies(cert_config_.policy_oids);
    if (intermediate)
      intermediate->SetCertificatePolicies(cert_config_.policy_oids);
  }

  if (!cert_config_.dns_names.empty() || !cert_config_.ip_addresses.empty()) {
    leaf->SetSubjectAltNames(cert_config_.dns_names, cert_config_.ip_addresses);
  } else {
    leaf->SetSubjectAltNames({}, {net::IPAddress::IPv4Localhost()});
  }

  if (!cert_config_.key_usages.empty()) {
    leaf->SetKeyUsages(cert_config_.key_usages);
  } else {
    leaf->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
  }

  if (!cert_config_.embedded_scts.empty()) {
    leaf->SetSctConfig(cert_config_.embedded_scts);
  }

  const std::string leaf_serial_text =
      base::NumberToString(leaf->GetSerialNumber());
  const std::string intermediate_serial_text =
      intermediate ? base::NumberToString(intermediate->GetSerialNumber()) : "";

  std::string ocsp_response;
  if (!MaybeCreateOCSPResponse(leaf.get(), cert_config_.ocsp_config,
                               &ocsp_response)) {
    return false;
  }
  if (!ocsp_response.empty()) {
    std::string ocsp_path = "/ocsp/" + leaf_serial_text;
    leaf_ocsp_urls.push_back(aia_http_server_->GetURL(ocsp_path));
    aia_http_server_->RegisterRequestHandler(
        base::BindRepeating(ServeResponseForSubPaths, ocsp_path, HTTP_OK,
                            "application/ocsp-response", ocsp_response));
  }

  std::string stapled_ocsp_response;
  if (!MaybeCreateOCSPResponse(leaf.get(), cert_config_.stapled_ocsp_config,
                               &stapled_ocsp_response)) {
    return false;
  }
  if (!stapled_ocsp_response.empty()) {
    ssl_config_.ocsp_response = std::vector<uint8_t>(
        stapled_ocsp_response.begin(), stapled_ocsp_response.end());
  }

  std::string intermediate_ocsp_response;
  if (!MaybeCreateOCSPResponse(intermediate.get(),
                               cert_config_.intermediate_ocsp_config,
                               &intermediate_ocsp_response)) {
    return false;
  }
  if (!intermediate_ocsp_response.empty()) {
    std::string intermediate_ocsp_path = "/ocsp/" + intermediate_serial_text;
    intermediate->SetCaIssuersAndOCSPUrls(
        {}, {aia_http_server_->GetURL(intermediate_ocsp_path)});
    aia_http_server_->RegisterRequestHandler(base::BindRepeating(
        ServeResponseForSubPaths, intermediate_ocsp_path, HTTP_OK,
        "application/ocsp-response", intermediate_ocsp_response));
  }

  if (cert_config_.intermediate == IntermediateType::kByAIA) {
    std::string ca_issuers_path = "/ca_issuers/" + intermediate_serial_text;
    leaf_ca_issuers_urls.push_back(aia_http_server_->GetURL(ca_issuers_path));

    // Setup AIA server to serve the intermediate referred to by the leaf.
    aia_http_server_->RegisterRequestHandler(
        base::BindRepeating(ServeResponseForPath, ca_issuers_path, HTTP_OK,
                            "application/pkix-cert", intermediate->GetDER()));
  }

  if (!leaf_ca_issuers_urls.empty() || !leaf_ocsp_urls.empty()) {
    leaf->SetCaIssuersAndOCSPUrls(leaf_ca_issuers_urls, leaf_ocsp_urls);
  }

  if (cert_config_.intermediate == IntermediateType::kByAIA ||
      cert_config_.intermediate == IntermediateType::kMissing) {
    // Server certificate chain does not include the intermediate.
    x509_cert_ = leaf->GetX509Certificate();
  } else {
    // Server certificate chain will include the intermediate, if there is one.
    x509_cert_ = leaf->GetX509CertificateChain();
  }

  if (intermediate) {
    intermediate_ = intermediate->GetX509Certificate();
  }

  root_ = root->GetX509Certificate();

  private_key_ = bssl::UpRef(leaf->GetKey());

  // If this server is already accepting connections but is being reconfigured,
  // start the new AIA server now. Otherwise, wait until
  // StartAcceptingConnections so that this server and the AIA server start at
  // the same time. (If the test only called InitializeAndListen they expect no
  // threads to be created yet.)
  if (io_thread_)
    aia_http_server_->StartAcceptingConnections();

  return true;
}

bool EmbeddedTestServer::InitializeSSLServerContext() {
  if (UsingStaticCert()) {
    if (!InitializeCertAndKeyFromFile())
      return false;
  } else {
    if (!GenerateCertAndKey())
      return false;
  }

  if (protocol_ == HttpConnection::Protocol::kHttp2) {
    ssl_config_.alpn_protos = {NextProto::kProtoHTTP2};
    if (!alps_accept_ch_.empty()) {
      base::StringPairs origin_accept_ch;
      size_t frame_size = spdy::kFrameHeaderSize;
      // Figure out size and generate origins
      for (const auto& pair : alps_accept_ch_) {
        std::string_view hostname = pair.first;
        std::string accept_ch = pair.second;

        GURL url = hostname.empty() ? GetURL("/") : GetURL(hostname, "/");
        std::string origin = url::Origin::Create(url).Serialize();

        frame_size += accept_ch.size() + origin.size() +
                      (sizeof(uint16_t) * 2);  // = Origin-Len + Value-Len

        origin_accept_ch.push_back({std::move(origin), std::move(accept_ch)});
      }

      spdy::SpdyFrameBuilder builder(frame_size);
      builder.BeginNewFrame(spdy::SpdyFrameType::ACCEPT_CH, 0, 0);
      for (const auto& pair : origin_accept_ch) {
        std::string_view origin = pair.first;
        std::string_view accept_ch = pair.second;

        builder.WriteUInt16(origin.size());
        builder.WriteBytes(origin.data(), origin.size());

        builder.WriteUInt16(accept_ch.size());
        builder.WriteBytes(accept_ch.data(), accept_ch.size());
      }

      spdy::SpdySerializedFrame serialized_frame = builder.take();
      DCHECK_EQ(frame_size, serialized_frame.size());

      ssl_config_.application_settings[NextProto::kProtoHTTP2] =
          std::vector<uint8_t>(
              serialized_frame.data(),
              serialized_frame.data() + serialized_frame.size());

      ssl_config_.client_hello_callback_for_testing =
          base::BindRepeating([](const SSL_CLIENT_HELLO* client_hello) {
            // Configure the server to use the ALPS codepoint that the client
            // offered.
            const uint8_t* unused_extension_bytes;
            size_t unused_extension_len;
            int use_alps_new_codepoint = SSL_early_callback_ctx_extension_get(
                client_hello, TLSEXT_TYPE_application_settings,
                &unused_extension_bytes, &unused_extension_len);
            // Make sure we use the right ALPS codepoint.
            SSL_set_alps_use_new_codepoint(client_hello->ssl,
                                           use_alps_new_codepoint);
            return true;
          });
    }
  }

  context_ =
      CreateSSLServerContext(x509_cert_.get(), private_key_.get(), ssl_config_);
  return true;
}

EmbeddedTestServerHandle
EmbeddedTestServer::StartAcceptingConnectionsAndReturnHandle() {
  StartAcceptingConnections();
  return EmbeddedTestServerHandle(this);
}

void EmbeddedTestServer::StartAcceptingConnections() {
  DCHECK(Started());
  DCHECK(!io_thread_) << "Server must not be started while server is running";

  if (aia_http_server_)
    aia_http_server_->StartAcceptingConnections();

  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  io_thread_ = std::make_unique<base::Thread>("EmbeddedTestServer IO Thread");
  CHECK(io_thread_->StartWithOptions(std::move(thread_options)));
  CHECK(io_thread_->WaitUntilThreadStarted());

  io_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EmbeddedTestServer::DoAcceptLoop,
                                base::Unretained(this)));
}

bool EmbeddedTestServer::ShutdownAndWaitUntilComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!io_thread_) {
    // Can't stop a server that never started.
    return true;
  }

  // Ensure that the AIA HTTP server is no longer Started().
  bool aia_http_server_not_started = true;
  if (aia_http_server_ && aia_http_server_->Started()) {
    aia_http_server_not_started =
        aia_http_server_->ShutdownAndWaitUntilComplete();
  }

  // Return false if either this or the AIA HTTP server are still Started().
  return PostTaskToIOThreadAndWait(
             base::BindOnce(&EmbeddedTestServer::ShutdownOnIOThread,
                            base::Unretained(this))) &&
         aia_http_server_not_started;
}

// static
base::FilePath EmbeddedTestServer::GetRootCertPemPath() {
  return GetTestCertsDirectory().AppendASCII("root_ca_cert.pem");
}

void EmbeddedTestServer::ShutdownOnIOThread() {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
  listen_socket_.reset();
  connections_.clear();
}

HttpConnection* EmbeddedTestServer::GetConnectionForSocket(
    const StreamSocket* socket) {
  auto it = connections_.find(socket);
  if (it != connections_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void EmbeddedTestServer::HandleRequest(
    base::WeakPtr<HttpResponseDelegate> delegate,
    std::unique_ptr<HttpRequest> request,
    const StreamSocket* socket) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  request->base_url = base_url_;

  for (const auto& monitor : request_monitors_)
    monitor.Run(*request);

  HttpConnection* connection = GetConnectionForSocket(socket);
  CHECK(connection);

  for (const auto& upgrade_request_handler : upgrade_request_handlers_) {
    auto upgrade_response = upgrade_request_handler.Run(*request, connection);
    if (upgrade_response.has_value()) {
      if (upgrade_response.value() == UpgradeResult::kUpgraded) {
        connections_.erase(socket);
        return;
      }
    } else {
      CHECK(upgrade_response.error());
      DispatchResponseToDelegate(std::move(upgrade_response.error()), delegate);
      return;
    }
  }

  std::unique_ptr<HttpResponse> response;

  for (const auto& handler : request_handlers_) {
    response = handler.Run(*request);
    if (response)
      break;
  }

  if (!response) {
    for (const auto& handler : default_request_handlers_) {
      response = handler.Run(*request);
      if (response)
        break;
    }
  }

  if (!response) {
    LOG(WARNING) << "Request not handled. Returning 404: "
                 << request->relative_url;
    auto not_found_response = std::make_unique<BasicHttpResponse>();
    not_found_response->set_code(HTTP_NOT_FOUND);
    response = std::move(not_found_response);
  }

  DispatchResponseToDelegate(std::move(response), delegate);
}

GURL EmbeddedTestServer::GetURL(std::string_view relative_url) const {
  DCHECK(Started()) << "You must start the server first.";
  DCHECK(relative_url.starts_with("/")) << relative_url;
  return base_url_.Resolve(relative_url);
}

GURL EmbeddedTestServer::GetURL(std::string_view hostname,
                                std::string_view relative_url) const {
  GURL local_url = GetURL(relative_url);
  GURL::Replacements replace_host;
  replace_host.SetHostStr(hostname);
  return local_url.ReplaceComponents(replace_host);
}

url::Origin EmbeddedTestServer::GetOrigin(
    const std::optional<std::string>& hostname) const {
  if (hostname)
    return url::Origin::Create(GetURL(*hostname, "/"));
  return url::Origin::Create(base_url_);
}

bool EmbeddedTestServer::GetAddressList(AddressList* address_list) const {
  *address_list = AddressList(local_endpoint_);
  return true;
}

std::string EmbeddedTestServer::GetIPLiteralString() const {
  return local_endpoint_.address().ToString();
}

void EmbeddedTestServer::SetSSLConfigInternal(
    ServerCertificate cert,
    const ServerCertificateConfig* cert_config,
    const SSLServerConfig& ssl_config) {
  DCHECK(!Started());
  cert_ = cert;
  DCHECK(!cert_config || cert == CERT_AUTO);
  cert_config_ = cert_config ? *cert_config : ServerCertificateConfig();
  x509_cert_ = nullptr;
  private_key_ = nullptr;
  ssl_config_ = ssl_config;
}

void EmbeddedTestServer::SetSSLConfig(ServerCertificate cert,
                                      const SSLServerConfig& ssl_config) {
  SetSSLConfigInternal(cert, /*cert_config=*/nullptr, ssl_config);
}

void EmbeddedTestServer::SetSSLConfig(ServerCertificate cert) {
  SetSSLConfigInternal(cert, /*cert_config=*/nullptr, SSLServerConfig());
}

void EmbeddedTestServer::SetSSLConfig(
    const ServerCertificateConfig& cert_config,
    const SSLServerConfig& ssl_config) {
  SetSSLConfigInternal(CERT_AUTO, &cert_config, ssl_config);
}

void EmbeddedTestServer::SetSSLConfig(
    const ServerCertificateConfig& cert_config) {
  SetSSLConfigInternal(CERT_AUTO, &cert_config, SSLServerConfig());
}

void EmbeddedTestServer::SetCertHostnames(std::vector<std::string> hostnames) {
  ServerCertificateConfig cert_config;
  cert_config.dns_names = std::move(hostnames);
  cert_config.ip_addresses = {net::IPAddress::IPv4Localhost()};
  SetSSLConfig(cert_config);
}

bool EmbeddedTestServer::ResetSSLConfigOnIOThread(
    ServerCertificate cert,
    const SSLServerConfig& ssl_config) {
  cert_ = cert;
  cert_config_ = ServerCertificateConfig();
  ssl_config_ = ssl_config;
  connections_.clear();
  return InitializeSSLServerContext();
}

bool EmbeddedTestServer::ResetSSLConfig(ServerCertificate cert,
                                        const SSLServerConfig& ssl_config) {
  return PostTaskToIOThreadAndWaitWithResult(
      base::BindOnce(&EmbeddedTestServer::ResetSSLConfigOnIOThread,
                     base::Unretained(this), cert, ssl_config));
}

std::string EmbeddedTestServer::GetCertificateName() const {
  DCHECK(is_using_ssl_);
  switch (cert_) {
    case CERT_OK:
    case CERT_MISMATCHED_NAME:
      return "ok_cert.pem";
    case CERT_COMMON_NAME_IS_DOMAIN:
      return "localhost_cert.pem";
    case CERT_EXPIRED:
      return "expired_cert.pem";
    case CERT_CHAIN_WRONG_ROOT:
      // This chain uses its own dedicated test root certificate to avoid
      // side-effects that may affect testing.
      return "redundant-server-chain.pem";
    case CERT_COMMON_NAME_ONLY:
      return "common_name_only.pem";
    case CERT_SHA1_LEAF:
      return "sha1_leaf.pem";
    case CERT_OK_BY_INTERMEDIATE:
      return "ok_cert_by_intermediate.pem";
    case CERT_BAD_VALIDITY:
      return "bad_validity.pem";
    case CERT_TEST_NAMES:
      return "test_names.pem";
    case CERT_KEY_USAGE_RSA_ENCIPHERMENT:
      return "key_usage_rsa_keyencipherment.pem";
    case CERT_KEY_USAGE_RSA_DIGITAL_SIGNATURE:
      return "key_usage_rsa_digitalsignature.pem";
    case CERT_AUTO:
      return std::string();
  }

  return "ok_cert.pem";
}

scoped_refptr<X509Certificate> EmbeddedTestServer::GetCertificate() {
  DCHECK(is_using_ssl_);
  if (!x509_cert_) {
    // Some tests want to get the certificate before the server has been
    // initialized, so load it now if necessary. This is only possible if using
    // a static certificate.
    // TODO(mattm): change contract to require initializing first in all cases,
    // update callers.
    CHECK(UsingStaticCert());
    // TODO(mattm): change contract to return nullptr on error instead of
    // CHECKing, update callers.
    CHECK(InitializeCertAndKeyFromFile());
  }
  return x509_cert_;
}

scoped_refptr<X509Certificate> EmbeddedTestServer::GetGeneratedIntermediate() {
  DCHECK(is_using_ssl_);
  DCHECK(!UsingStaticCert());
  return intermediate_;
}

scoped_refptr<X509Certificate> EmbeddedTestServer::GetRoot() {
  DCHECK(is_using_ssl_);
  return root_;
}

void EmbeddedTestServer::ServeFilesFromDirectory(
    const base::FilePath& directory) {
  RegisterDefaultHandler(base::BindRepeating(&HandleFileRequest, directory));
}

void EmbeddedTestServer::ServeFilesFromSourceDirectory(
    std::string_view relative) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  ServeFilesFromDirectory(test_data_dir.AppendASCII(relative));
}

void EmbeddedTestServer::ServeFilesFromSourceDirectory(
    const base::FilePath& relative) {
  ServeFilesFromDirectory(GetFullPathFromSourceDirectory(relative));
}

void EmbeddedTestServer::AddDefaultHandlers(const base::FilePath& directory) {
  ServeFilesFromSourceDirectory(directory);
  AddDefaultHandlers();
}

void EmbeddedTestServer::AddDefaultHandlers() {
  RegisterDefaultHandlers(this);
}

base::FilePath EmbeddedTestServer::GetFullPathFromSourceDirectory(
    const base::FilePath& relative) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  return test_data_dir.Append(relative);
}

void EmbeddedTestServer::RegisterUpgradeRequestHandler(
    const HandleUpgradeRequestCallback& callback) {
  CHECK_NE(protocol_, HttpConnection::Protocol::kHttp2)
      << "RegisterUpgradeRequestHandler() is not supported for HTTP/2 "
         "connections";
  CHECK(!io_thread_)
      << "Handlers must be registered before starting the server.";
  upgrade_request_handlers_.push_back(callback);
}

void EmbeddedTestServer::RegisterRequestHandler(
    const HandleRequestCallback& callback) {
  DCHECK(!io_thread_)
      << "Handlers must be registered before starting the server.";
  request_handlers_.push_back(callback);
}

void EmbeddedTestServer::RegisterRequestMonitor(
    const MonitorRequestCallback& callback) {
  DCHECK(!io_thread_)
      << "Monitors must be registered before starting the server.";
  request_monitors_.push_back(callback);
}

void EmbeddedTestServer::RegisterDefaultHandler(
    const HandleRequestCallback& callback) {
  DCHECK(!io_thread_)
      << "Handlers must be registered before starting the server.";
  default_request_handlers_.push_back(callback);
}

std::unique_ptr<SSLServerSocket> EmbeddedTestServer::DoSSLUpgrade(
    std::unique_ptr<StreamSocket> connection) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());

  return context_->CreateSSLServerSocket(std::move(connection));
}

void EmbeddedTestServer::DoAcceptLoop() {
  while (true) {
    int rv = listen_socket_->Accept(
        &accepted_socket_,
        base::BindOnce(&EmbeddedTestServer::OnAcceptCompleted,
                       base::Unretained(this)));
    if (rv != OK)
      return;

    HandleAcceptResult(std::move(accepted_socket_));
  }
}

bool EmbeddedTestServer::FlushAllSocketsAndConnectionsOnUIThread() {
  return PostTaskToIOThreadAndWait(
      base::BindOnce(&EmbeddedTestServer::FlushAllSocketsAndConnections,
                     base::Unretained(this)));
}

void EmbeddedTestServer::FlushAllSocketsAndConnections() {
  connections_.clear();
}

void EmbeddedTestServer::SetAlpsAcceptCH(std::string hostname,
                                         std::string accept_ch) {
  alps_accept_ch_.insert_or_assign(std::move(hostname), std::move(accept_ch));
}

void EmbeddedTestServer::OnAcceptCompleted(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  HandleAcceptResult(std::move(accepted_socket_));
  DoAcceptLoop();
}

void EmbeddedTestServer::OnHandshakeDone(HttpConnection* connection, int rv) {
  if (connection->Socket()->IsConnected()) {
    connection->OnSocketReady();
  } else {
    RemoveConnection(connection);
  }
}

void EmbeddedTestServer::HandleAcceptResult(
    std::unique_ptr<StreamSocket> socket_ptr) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  if (connection_listener_)
    socket_ptr = connection_listener_->AcceptedSocket(std::move(socket_ptr));

  if (!is_using_ssl_) {
    AddConnection(std::move(socket_ptr))->OnSocketReady();
    return;
  }

  socket_ptr = DoSSLUpgrade(std::move(socket_ptr));

  StreamSocket* socket = socket_ptr.get();
  HttpConnection* connection = AddConnection(std::move(socket_ptr));

  int rv = static_cast<SSLServerSocket*>(socket)->Handshake(
      base::BindOnce(&EmbeddedTestServer::OnHandshakeDone,
                     base::Unretained(this), connection));
  if (rv != ERR_IO_PENDING)
    OnHandshakeDone(connection, rv);
}

HttpConnection* EmbeddedTestServer::AddConnection(
    std::unique_ptr<StreamSocket> socket_ptr) {
  StreamSocket* socket = socket_ptr.get();
  std::unique_ptr<HttpConnection> connection_ptr = HttpConnection::Create(
      std::move(socket_ptr), connection_listener_, this, protocol_);
  HttpConnection* connection = connection_ptr.get();
  connections_[socket] = std::move(connection_ptr);

  return connection;
}

void EmbeddedTestServer::RemoveConnection(
    HttpConnection* connection,
    EmbeddedTestServerConnectionListener* listener) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  DCHECK(connection);
  DCHECK_EQ(1u, connections_.count(connection->Socket()));

  StreamSocket* raw_socket = connection->Socket();
  std::unique_ptr<StreamSocket> socket = connection->TakeSocket();
  connections_.erase(raw_socket);

  if (listener && socket && socket->IsConnected())
    listener->OnResponseCompletedSuccessfully(std::move(socket));
}

bool EmbeddedTestServer::PostTaskToIOThreadAndWait(base::OnceClosure closure) {
  // Note that PostTaskAndReply below requires
  // base::SingleThreadTaskRunner::GetCurrentDefault() to return a task runner
  // for posting the reply task. However, in order to make EmbeddedTestServer
  // universally usable, it needs to cope with the situation where it's running
  // on a thread on which a task executor is not (yet) available or has been
  // destroyed already.
  //
  // To handle this situation, create temporary task executor to support the
  // PostTaskAndReply operation if the current thread has no task executor.
  // TODO(mattm): Is this still necessary/desirable? Try removing this and see
  // if anything breaks.
  std::unique_ptr<base::SingleThreadTaskExecutor> temporary_loop;
  if (!base::CurrentThread::Get())
    temporary_loop = std::make_unique<base::SingleThreadTaskExecutor>();

  base::RunLoop run_loop;
  if (!io_thread_->task_runner()->PostTaskAndReply(
          FROM_HERE, std::move(closure), run_loop.QuitClosure())) {
    return false;
  }
  run_loop.Run();

  return true;
}

bool EmbeddedTestServer::PostTaskToIOThreadAndWaitWithResult(
    base::OnceCallback<bool()> task) {
  // Note that PostTaskAndReply below requires
  // base::SingleThreadTaskRunner::GetCurrentDefault() to return a task runner
  // for posting the reply task. However, in order to make EmbeddedTestServer
  // universally usable, it needs to cope with the situation where it's running
  // on a thread on which a task executor is not (yet) available or has been
  // destroyed already.
  //
  // To handle this situation, create temporary task executor to support the
  // PostTaskAndReply operation if the current thread has no task executor.
  // TODO(mattm): Is this still necessary/desirable? Try removing this and see
  // if anything breaks.
  std::unique_ptr<base::SingleThreadTaskExecutor> temporary_loop;
  if (!base::CurrentThread::Get())
    temporary_loop = std::make_unique<base::SingleThreadTaskExecutor>();

  base::RunLoop run_loop;
  bool task_result = false;
  if (!io_thread_->task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, std::move(task),
          base::BindOnce(base::BindLambdaForTesting([&](bool result) {
            task_result = result;
            run_loop.Quit();
          })))) {
    return false;
  }
  run_loop.Run();

  return task_result;
}

}  // namespace net::test_server
