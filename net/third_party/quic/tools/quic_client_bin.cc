// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.
// Connects to a host using QUIC, sends a request to the provided URL, and
// displays the response.
//
// Some usage examples:
//
//   TODO(rtenneti): make --host optional by getting IP Address of URL's host.
//
//   Get IP address of the www.google.com
//   IP=`dig www.google.com +short | head -1`
//
// Standard request/response:
//   quic_client http://www.google.com  --host=${IP}
//   quic_client http://www.google.com --quiet  --host=${IP}
//   quic_client https://www.google.com --port=443  --host=${IP}
//
// Use a specific version:
//   quic_client http://www.google.com --quic_version=23  --host=${IP}
//
// Send a POST instead of a GET:
//   quic_client http://www.google.com --body="this is a POST body" --host=${IP}
//
// Append additional headers to the request:
//   quic_client http://www.google.com  --host=${IP}
//               --headers="Header-A: 1234; Header-B: 5678"
//
// Connect to a host different to the URL being requested:
//   Get IP address of the www.google.com
//   IP=`dig www.google.com +short | head -1`
//   quic_client mail.google.com --host=${IP}
//
// Try to connect to a host which does not speak QUIC:
//   Get IP address of the www.example.com
//   IP=`dig www.example.com +short | head -1`
//   quic_client http://www.example.com --host=${IP}

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/tools/quic_client.h"
#include "net/third_party/quic/tools/quic_url.h"
#include "net/third_party/spdy/core/spdy_header_block.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/synchronous_host_resolver.h"

using net::CertVerifier;
using net::CTVerifier;
using net::MultiLogCTVerifier;
using net::ProofVerifierChromium;
using quic::ProofVerifier;
using quic::QuicStringPiece;
using quic::QuicTextUtils;
using quic::QuicUrl;
using spdy::SpdyHeaderBlock;
using net::TransportSecurityState;
using std::cout;
using std::cerr;
using std::endl;
using std::string;

// The IP or hostname the quic client will connect to.
string FLAGS_host = "";
// The port to connect to.
int32_t FLAGS_port = 0;
// If set, send a POST with this body.
string FLAGS_body = "";
// If set, contents are converted from hex to ascii, before sending as body of
// a POST. e.g. --body_hex=\"68656c6c6f\"
string FLAGS_body_hex = "";
// A semicolon separated list of key:value pairs to add to request headers.
string FLAGS_headers = "";
// Set to true for a quieter output experience.
bool FLAGS_quiet = false;
// QUIC version to speak, e.g. 21. If not set, then all available versions are
// offered in the handshake.
int32_t FLAGS_quic_version = -1;
// If true, a version mismatch in the handshake is not considered a failure.
// Useful for probing a server to determine if it speaks any version of QUIC.
bool FLAGS_version_mismatch_ok = false;
// If true, an HTTP response code of 3xx is considered to be a successful
// response, otherwise a failure.
bool FLAGS_redirect_is_success = true;
// Initial MTU of the connection.
int32_t FLAGS_initial_mtu = 0;
// If true, drop response body immediately after it is received.
bool FLAGS_drop_response_body = false;

class FakeProofVerifier : public ProofVerifier {
 public:
  quic::QuicAsyncStatus VerifyProof(
      const string& /*hostname*/,
      const uint16_t /*port*/,
      const string& /*server_config*/,
      quic::QuicTransportVersion /*quic_version*/,
      QuicStringPiece /*chlo_hash*/,
      const std::vector<string>& /*certs*/,
      const string& /*cert_sct*/,
      const string& /*signature*/,
      const quic::ProofVerifyContext* /*context*/,
      string* /*error_details*/,
      std::unique_ptr<quic::ProofVerifyDetails>* /*details*/,
      std::unique_ptr<quic::ProofVerifierCallback> /*callback*/) override {
    return quic::QUIC_SUCCESS;
  }

  quic::QuicAsyncStatus VerifyCertChain(
      const std::string& /*hostname*/,
      const std::vector<std::string>& /*certs*/,
      const quic::ProofVerifyContext* /*verify_context*/,
      std::string* /*error_details*/,
      std::unique_ptr<quic::ProofVerifyDetails>* /*verify_details*/,
      std::unique_ptr<quic::ProofVerifierCallback> /*callback*/) override {
    return quic::QUIC_SUCCESS;
  }
  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};

int main(int argc, char* argv[]) {
  base::TaskScheduler::CreateAndStartWithDefaultParams("quic_client");
  base::CommandLine::Init(argc, argv);
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& urls = line->GetArgs();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  if (line->HasSwitch("h") || line->HasSwitch("help") || urls.empty()) {
    const char* help_str =
        "Usage: quic_client [options] <url>\n"
        "\n"
        "<url> with scheme must be provided (e.g. http://www.google.com)\n\n"
        "Options:\n"
        "-h, --help                  show this help message and exit\n"
        "--host=<host>               specify the IP address of the hostname to "
        "connect to\n"
        "--port=<port>               specify the port to connect to\n"
        "--body=<body>               specify the body to post\n"
        "--body_hex=<body_hex>       specify the body_hex to be printed out\n"
        "--headers=<headers>         specify a semicolon separated list of "
        "key:value pairs to add to request headers\n"
        "--quiet                     specify for a quieter output experience\n"
        "--quic-version=<quic version> specify QUIC version to speak\n"
        "--version_mismatch_ok       if specified a version mismatch in the "
        "handshake is not considered a failure\n"
        "--redirect_is_success       if specified an HTTP response code of 3xx "
        "is considered to be a successful response, otherwise a failure\n"
        "--initial_mtu=<initial_mtu> specify the initial MTU of the connection"
        "\n"
        "--disable-certificate-verification do not verify certificates\n";
    cout << help_str;
    exit(0);
  }
  if (line->HasSwitch("host")) {
    FLAGS_host = line->GetSwitchValueASCII("host");
  }
  if (line->HasSwitch("port")) {
    if (!base::StringToInt(line->GetSwitchValueASCII("port"), &FLAGS_port)) {
      std::cerr << "--port must be an integer\n";
      return 1;
    }
  }
  if (line->HasSwitch("body")) {
    FLAGS_body = line->GetSwitchValueASCII("body");
  }
  if (line->HasSwitch("body_hex")) {
    FLAGS_body_hex = line->GetSwitchValueASCII("body_hex");
  }
  if (line->HasSwitch("headers")) {
    FLAGS_headers = line->GetSwitchValueASCII("headers");
  }
  if (line->HasSwitch("quiet")) {
    FLAGS_quiet = true;
  }
  if (line->HasSwitch("quic-version")) {
    int quic_version;
    if (base::StringToInt(line->GetSwitchValueASCII("quic-version"),
                          &quic_version)) {
      FLAGS_quic_version = quic_version;
    }
  }
  if (line->HasSwitch("version_mismatch_ok")) {
    FLAGS_version_mismatch_ok = true;
  }
  if (line->HasSwitch("redirect_is_success")) {
    FLAGS_redirect_is_success = true;
  }
  if (line->HasSwitch("initial_mtu")) {
    if (!base::StringToInt(line->GetSwitchValueASCII("initial_mtu"),
                           &FLAGS_initial_mtu)) {
      std::cerr << "--initial_mtu must be an integer\n";
      return 1;
    }
  }
  if (line->HasSwitch("drop_response_body")) {
    FLAGS_drop_response_body = true;
  }

  VLOG(1) << "server host: " << FLAGS_host << " port: " << FLAGS_port
          << " body: " << FLAGS_body << " headers: " << FLAGS_headers
          << " quiet: " << FLAGS_quiet
          << " quic-version: " << FLAGS_quic_version
          << " version_mismatch_ok: " << FLAGS_version_mismatch_ok
          << " redirect_is_success: " << FLAGS_redirect_is_success
          << " initial_mtu: " << FLAGS_initial_mtu;

  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  // Determine IP address to connect to from supplied hostname.
  quic::QuicIpAddress ip_addr;

  QuicUrl url(urls[0], "https");
  string host = FLAGS_host;
  if (host.empty()) {
    host = url.host();
  }
  int port = FLAGS_port;
  if (port == 0) {
    port = url.port();
  }
  if (!ip_addr.FromString(host)) {
    net::AddressList addresses;
    int rv = net::SynchronousHostResolver::Resolve(host, &addresses);
    if (rv != net::OK) {
      LOG(ERROR) << "Unable to resolve '" << host
                 << "' : " << net::ErrorToShortString(rv);
      return 1;
    }
    ip_addr =
        quic::QuicIpAddress(quic::QuicIpAddressImpl(addresses[0].address()));
  }

  string host_port = quic::QuicStrCat(ip_addr.ToString(), ":", port);
  VLOG(1) << "Resolved " << host << " to " << host_port << endl;

  // Build the client, and try to connect.
  net::EpollServer epoll_server;
  quic::QuicServerId server_id(url.host(), url.port(), false);
  quic::ParsedQuicVersionVector versions = quic::CurrentSupportedVersions();
  if (FLAGS_quic_version != -1) {
    versions.clear();
    versions.push_back(quic::ParsedQuicVersion(
        quic::PROTOCOL_QUIC_CRYPTO,
        static_cast<quic::QuicTransportVersion>(FLAGS_quic_version)));
  }
  // For secure QUIC we need to verify the cert chain.
  std::unique_ptr<CertVerifier> cert_verifier(CertVerifier::CreateDefault());
  std::unique_ptr<TransportSecurityState> transport_security_state(
      new TransportSecurityState);
  std::unique_ptr<MultiLogCTVerifier> ct_verifier(new MultiLogCTVerifier());
  std::unique_ptr<net::CTPolicyEnforcer> ct_policy_enforcer(
      new net::DefaultCTPolicyEnforcer());
  std::unique_ptr<ProofVerifier> proof_verifier;
  if (line->HasSwitch("disable-certificate-verification")) {
    proof_verifier = quic::QuicMakeUnique<FakeProofVerifier>();
  } else {
    proof_verifier = quic::QuicMakeUnique<ProofVerifierChromium>(
        cert_verifier.get(), ct_policy_enforcer.get(),
        transport_security_state.get(), ct_verifier.get());
  }
  quic::QuicClient client(quic::QuicSocketAddress(ip_addr, port), server_id,
                          versions, &epoll_server, std::move(proof_verifier));
  client.set_initial_max_packet_length(
      FLAGS_initial_mtu != 0 ? FLAGS_initial_mtu : quic::kDefaultMaxPacketSize);
  client.set_drop_response_body(FLAGS_drop_response_body);
  if (!client.Initialize()) {
    cerr << "Failed to initialize client." << endl;
    return 1;
  }
  if (!client.Connect()) {
    quic::QuicErrorCode error = client.session()->error();
    if (error == quic::QUIC_INVALID_VERSION) {
      cout << "Server talks QUIC, but none of the versions supported by "
           << "this client: " << ParsedQuicVersionVectorToString(versions)
           << endl;
      // 0: No error.
      // 20: Failed to connect due to QUIC_INVALID_VERSION.
      return FLAGS_version_mismatch_ok ? 0 : 20;
    }
    cerr << "Failed to connect to " << host_port
         << ". Error: " << quic::QuicErrorCodeToString(error) << endl;
    return 1;
  }
  cout << "Connected to " << host_port << endl;

  // Construct the string body from flags, if provided.
  string body = FLAGS_body;
  if (!FLAGS_body_hex.empty()) {
    DCHECK(FLAGS_body.empty()) << "Only set one of --body and --body_hex.";
    body = QuicTextUtils::HexDecode(FLAGS_body_hex);
  }

  // Construct a GET or POST request for supplied URL.
  spdy::SpdyHeaderBlock header_block;
  header_block[":method"] = body.empty() ? "GET" : "POST";
  header_block[":scheme"] = url.scheme();
  header_block[":authority"] = url.HostPort();
  header_block[":path"] = url.PathParamsQuery();

  // Append any additional headers supplied on the command line.
  for (QuicStringPiece sp : QuicTextUtils::Split(FLAGS_headers, ';')) {
    QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&sp);
    if (sp.empty()) {
      continue;
    }
    std::vector<QuicStringPiece> kv = QuicTextUtils::Split(sp, ':');
    QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[0]);
    QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[1]);
    header_block[kv[0]] = kv[1];
  }

  // Make sure to store the response, for later output.
  client.set_store_response(true);

  // Send the request.
  client.SendRequestAndWaitForResponse(header_block, body, /*fin=*/true);

  // Print request and response details.
  if (!FLAGS_quiet) {
    cout << "Request:" << endl;
    cout << "headers:" << header_block.DebugString();
    if (!FLAGS_body_hex.empty()) {
      // Print the user provided hex, rather than binary body.
      cout << "body:\n"
           << QuicTextUtils::HexDump(QuicTextUtils::HexDecode(FLAGS_body_hex))
           << endl;
    } else {
      cout << "body: " << body << endl;
    }
    cout << endl;

    if (!client.preliminary_response_headers().empty()) {
      cout << "Preliminary response headers: "
           << client.preliminary_response_headers() << endl;
      cout << endl;
    }

    cout << "Response:" << endl;
    cout << "headers: " << client.latest_response_headers() << endl;
    string response_body = client.latest_response_body();
    if (!FLAGS_body_hex.empty()) {
      // Assume response is binary data.
      cout << "body:\n" << QuicTextUtils::HexDump(response_body) << endl;
    } else {
      cout << "body: " << response_body << endl;
    }
    cout << "trailers: " << client.latest_response_trailers() << endl;
  }

  size_t response_code = client.latest_response_code();
  if (response_code >= 200 && response_code < 300) {
    cout << "Request succeeded (" << response_code << ")." << endl;
    return 0;
  } else if (response_code >= 300 && response_code < 400) {
    if (FLAGS_redirect_is_success) {
      cout << "Request succeeded (redirect " << response_code << ")." << endl;
      return 0;
    } else {
      cout << "Request failed (redirect " << response_code << ")." << endl;
      return 1;
    }
  } else {
    cerr << "Request failed (" << response_code << ")." << endl;
    return 1;
  }
}
