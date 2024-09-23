// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/base_test_server.h"

#include <stdint.h>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/port_util.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/public/dns_query_type.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "url/gurl.h"

namespace net {

namespace {

std::string GetHostname(BaseTestServer::Type type,
                        const BaseTestServer::SSLOptions& options) {
  if (BaseTestServer::UsingSSL(type)) {
    if (options.server_certificate ==
            BaseTestServer::SSLOptions::CERT_MISMATCHED_NAME ||
        options.server_certificate ==
            BaseTestServer::SSLOptions::CERT_COMMON_NAME_IS_DOMAIN) {
      // For |CERT_MISMATCHED_NAME|, return a different hostname string
      // that resolves to the same hostname. For
      // |CERT_COMMON_NAME_IS_DOMAIN|, the certificate is issued for
      // "localhost" instead of "127.0.0.1".
      return "localhost";
    }
  }

  return "127.0.0.1";
}

bool GetLocalCertificatesDir(const base::FilePath& certificates_dir,
                             base::FilePath* local_certificates_dir) {
  if (certificates_dir.IsAbsolute()) {
    *local_certificates_dir = certificates_dir;
    return true;
  }

  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir)) {
    return false;
  }

  *local_certificates_dir = src_dir.Append(certificates_dir);
  return true;
}

}  // namespace

BaseTestServer::SSLOptions::SSLOptions() = default;
BaseTestServer::SSLOptions::SSLOptions(ServerCertificate cert)
    : server_certificate(cert) {}
BaseTestServer::SSLOptions::SSLOptions(base::FilePath cert)
    : custom_certificate(std::move(cert)) {}
BaseTestServer::SSLOptions::SSLOptions(const SSLOptions& other) = default;

BaseTestServer::SSLOptions::~SSLOptions() = default;

base::FilePath BaseTestServer::SSLOptions::GetCertificateFile() const {
  if (!custom_certificate.empty())
    return custom_certificate;

  switch (server_certificate) {
    case CERT_OK:
    case CERT_MISMATCHED_NAME:
      return base::FilePath(FILE_PATH_LITERAL("ok_cert.pem"));
    case CERT_COMMON_NAME_IS_DOMAIN:
      return base::FilePath(FILE_PATH_LITERAL("localhost_cert.pem"));
    case CERT_EXPIRED:
      return base::FilePath(FILE_PATH_LITERAL("expired_cert.pem"));
    case CERT_CHAIN_WRONG_ROOT:
      // This chain uses its own dedicated test root certificate to avoid
      // side-effects that may affect testing.
      return base::FilePath(FILE_PATH_LITERAL("redundant-server-chain.pem"));
    case CERT_BAD_VALIDITY:
      return base::FilePath(FILE_PATH_LITERAL("bad_validity.pem"));
    case CERT_KEY_USAGE_RSA_ENCIPHERMENT:
      return base::FilePath(
          FILE_PATH_LITERAL("key_usage_rsa_keyencipherment.pem"));
    case CERT_KEY_USAGE_RSA_DIGITAL_SIGNATURE:
      return base::FilePath(
          FILE_PATH_LITERAL("key_usage_rsa_digitalsignature.pem"));
    case CERT_TEST_NAMES:
      return base::FilePath(FILE_PATH_LITERAL("test_names.pem"));
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return base::FilePath();
}

BaseTestServer::BaseTestServer(Type type) : type_(type) {
  Init(GetHostname(type, ssl_options_));
}

BaseTestServer::BaseTestServer(Type type, const SSLOptions& ssl_options)
    : ssl_options_(ssl_options), type_(type) {
  DCHECK(UsingSSL(type));
  Init(GetHostname(type, ssl_options));
}

BaseTestServer::~BaseTestServer() = default;

bool BaseTestServer::Start() {
  return StartInBackground() && BlockUntilStarted();
}

const HostPortPair& BaseTestServer::host_port_pair() const {
  DCHECK(started_);
  return host_port_pair_;
}

std::string BaseTestServer::GetScheme() const {
  switch (type_) {
    case TYPE_WS:
      return "ws";
    case TYPE_WSS:
      return "wss";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::string();
}

bool BaseTestServer::GetAddressList(AddressList* address_list) const {
  // Historically, this function did a DNS lookup because `host_port_pair_`
  // could specify something other than localhost. Now it is always localhost.
  DCHECK(host_port_pair_.host() == "127.0.0.1" ||
         host_port_pair_.host() == "localhost");
  DCHECK(address_list);
  *address_list = AddressList(
      IPEndPoint(IPAddress::IPv4Localhost(), host_port_pair_.port()));
  return true;
}

uint16_t BaseTestServer::GetPort() {
  return host_port_pair_.port();
}

void BaseTestServer::SetPort(uint16_t port) {
  host_port_pair_.set_port(port);
}

GURL BaseTestServer::GetURL(const std::string& path) const {
  return GURL(GetScheme() + "://" + host_port_pair_.ToString() + "/" + path);
}

GURL BaseTestServer::GetURL(const std::string& hostname,
                            const std::string& relative_url) const {
  GURL local_url = GetURL(relative_url);
  GURL::Replacements replace_host;
  replace_host.SetHostStr(hostname);
  return local_url.ReplaceComponents(replace_host);
}

GURL BaseTestServer::GetURLWithUser(const std::string& path,
                                const std::string& user) const {
  return GURL(GetScheme() + "://" + user + "@" + host_port_pair_.ToString() +
              "/" + path);
}

GURL BaseTestServer::GetURLWithUserAndPassword(const std::string& path,
                                           const std::string& user,
                                           const std::string& password) const {
  return GURL(GetScheme() + "://" + user + ":" + password + "@" +
              host_port_pair_.ToString() + "/" + path);
}

// static
bool BaseTestServer::GetFilePathWithReplacements(
    const std::string& original_file_path,
    const std::vector<StringPair>& text_to_replace,
    std::string* replacement_path) {
  std::string new_file_path = original_file_path;
  bool first_query_parameter = true;
  const std::vector<StringPair>::const_iterator end = text_to_replace.end();
  for (auto it = text_to_replace.begin(); it != end; ++it) {
    const std::string& old_text = it->first;
    const std::string& new_text = it->second;
    std::string base64_old = base::Base64Encode(old_text);
    std::string base64_new = base::Base64Encode(new_text);
    if (first_query_parameter) {
      new_file_path += "?";
      first_query_parameter = false;
    } else {
      new_file_path += "&";
    }
    new_file_path += "replace_text=";
    new_file_path += base64_old;
    new_file_path += ":";
    new_file_path += base64_new;
  }

  *replacement_path = new_file_path;
  return true;
}

ScopedTestRoot BaseTestServer::RegisterTestCerts() {
  auto root = ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  if (!root)
    return ScopedTestRoot();
  return ScopedTestRoot(CertificateList{root});
}

bool BaseTestServer::LoadTestRootCert() {
  scoped_test_root_ = RegisterTestCerts();
  return !scoped_test_root_.IsEmpty();
}

scoped_refptr<X509Certificate> BaseTestServer::GetCertificate() const {
  base::FilePath certificate_path;
  if (!GetLocalCertificatesDir(certificates_dir_, &certificate_path))
    return nullptr;

  base::FilePath certificate_file(ssl_options_.GetCertificateFile());
  if (certificate_file.value().empty())
    return nullptr;

  certificate_path = certificate_path.Append(certificate_file);

  std::string cert_data;
  if (!base::ReadFileToString(certificate_path, &cert_data))
    return nullptr;

  CertificateList certs_in_file =
      X509Certificate::CreateCertificateListFromBytes(
          base::as_byte_span(cert_data),
          X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  if (certs_in_file.empty())
    return nullptr;
  return certs_in_file[0];
}

void BaseTestServer::Init(const std::string& host) {
  host_port_pair_ = HostPortPair(host, 0);

  // TODO(battre) Remove this after figuring out why the TestServer is flaky.
  // http://crbug.com/96594
  log_to_console_ = true;
}

void BaseTestServer::SetResourcePath(const base::FilePath& document_root,
                                     const base::FilePath& certificates_dir) {
  // This method shouldn't get called twice.
  DCHECK(certificates_dir_.empty());
  document_root_ = document_root;
  certificates_dir_ = certificates_dir;
  DCHECK(!certificates_dir_.empty());
}

bool BaseTestServer::SetAndParseServerData(const std::string& server_data,
                                           int* port) {
  VLOG(1) << "Server data: " << server_data;
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(server_data);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Could not parse server data: "
               << parsed_json.error().message;
    return false;
  } else if (!parsed_json->is_dict()) {
    LOG(ERROR) << "Could not parse server data: expecting a dictionary";
    return false;
  }

  std::optional<int> port_value = parsed_json->GetDict().FindInt("port");
  if (!port_value) {
    LOG(ERROR) << "Could not find port value";
    return false;
  }

  *port = *port_value;
  if ((*port <= 0) || (*port > std::numeric_limits<uint16_t>::max())) {
    LOG(ERROR) << "Invalid port value: " << port;
    return false;
  }

  return true;
}

bool BaseTestServer::SetupWhenServerStarted() {
  DCHECK(host_port_pair_.port());
  DCHECK(!started_);

  if (UsingSSL(type_) && !LoadTestRootCert()) {
    LOG(ERROR) << "Could not load test root certificate.";
    return false;
  }

  started_ = true;
  allowed_port_ = std::make_unique<ScopedPortException>(host_port_pair_.port());
  return true;
}

void BaseTestServer::CleanUpWhenStoppingServer() {
  scoped_test_root_.Reset({});
  host_port_pair_.set_port(0);
  allowed_port_.reset();
  started_ = false;
}

std::optional<base::Value::Dict> BaseTestServer::GenerateArguments() const {
  base::Value::Dict arguments;
  arguments.Set("host", host_port_pair_.host());
  arguments.Set("port", host_port_pair_.port());
  arguments.Set("data-dir", document_root_.AsUTF8Unsafe());

  if (VLOG_IS_ON(1) || log_to_console_)
    arguments.Set("log-to-console", base::Value());

  if (ws_basic_auth_) {
    DCHECK(type_ == TYPE_WS || type_ == TYPE_WSS);
    arguments.Set("ws-basic-auth", base::Value());
  }

  if (redirect_connect_to_localhost_) {
    DCHECK(type_ == TYPE_BASIC_AUTH_PROXY || type_ == TYPE_PROXY);
    arguments.Set("redirect-connect-to-localhost", base::Value());
  }

  if (UsingSSL(type_)) {
    // Check the certificate arguments of the HTTPS server.
    base::FilePath certificate_path(certificates_dir_);
    base::FilePath certificate_file(ssl_options_.GetCertificateFile());
    if (!certificate_file.value().empty()) {
      certificate_path = certificate_path.Append(certificate_file);
      if (certificate_path.IsAbsolute() &&
          !base::PathExists(certificate_path)) {
        LOG(ERROR) << "Certificate path " << certificate_path.value()
                   << " doesn't exist. Can't launch https server.";
        return std::nullopt;
      }
      arguments.Set("cert-and-key-file", certificate_path.AsUTF8Unsafe());
    }

    // Check the client certificate related arguments.
    if (ssl_options_.request_client_certificate)
      arguments.Set("ssl-client-auth", base::Value());

    base::Value::List ssl_client_certs;

    std::vector<base::FilePath>::const_iterator it;
    for (it = ssl_options_.client_authorities.begin();
         it != ssl_options_.client_authorities.end(); ++it) {
      if (it->IsAbsolute() && !base::PathExists(*it)) {
        LOG(ERROR) << "Client authority path " << it->value()
                   << " doesn't exist. Can't launch https server.";
        return std::nullopt;
      }
      ssl_client_certs.Append(it->AsUTF8Unsafe());
    }

    if (ssl_client_certs.size()) {
      arguments.Set("ssl-client-ca", std::move(ssl_client_certs));
    }
  }

  return std::make_optional(std::move(arguments));
}

}  // namespace net
