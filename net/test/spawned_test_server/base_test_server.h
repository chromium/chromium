// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: spawned_test_server is deprecated, since it frequently causes test
// flakiness. Please consider using embedded_test_server if possible.

#ifndef NET_TEST_SPAWNED_TEST_SERVER_BASE_TEST_SERVER_H_
#define NET_TEST_SPAWNED_TEST_SERVER_BASE_TEST_SERVER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/cert/test_root_certs.h"

class GURL;

namespace net {

class AddressList;
class ScopedPortException;
class ScopedTestRoot;
class X509Certificate;

// The base class of Test server implementation.
class BaseTestServer {
 public:
  typedef std::pair<std::string, std::string> StringPair;

  enum Type {
    TYPE_BASIC_AUTH_PROXY,
    TYPE_WS,
    TYPE_WSS,
    TYPE_PROXY,
  };

  // Container for various options to control how the HTTPS or WSS server is
  // initialized.
  struct SSLOptions {
    enum ServerCertificate {
      CERT_OK,

      CERT_MISMATCHED_NAME,
      CERT_EXPIRED,
      // Cross-signed certificate to test PKIX path building. Contains an
      // intermediate cross-signed by an unknown root, while the client (via
      // TestRootStore) is expected to have a self-signed version of the
      // intermediate.
      CERT_CHAIN_WRONG_ROOT,

      // Causes the testserver to use a hostname that is a domain
      // instead of an IP.
      CERT_COMMON_NAME_IS_DOMAIN,

      // An RSA certificate with the keyUsage extension specifying that the key
      // is only for encipherment.
      CERT_KEY_USAGE_RSA_ENCIPHERMENT,

      // An RSA certificate with the keyUsage extension specifying that the key
      // is only for digital signatures.
      CERT_KEY_USAGE_RSA_DIGITAL_SIGNATURE,

      // A certificate with invalid notBefore and notAfter times. Windows'
      // certificate library will not parse this certificate.
      CERT_BAD_VALIDITY,

      // A certificate that covers a number of test names. See [test_names] in
      // net/data/ssl/scripts/ee.cnf. More may be added by editing this list and
      // and rerunning net/data/ssl/scripts/generate-test-certs.sh.
      CERT_TEST_NAMES,
    };

    // Initialize a new SSLOptions using CERT_OK as the certificate.
    SSLOptions();

    // Initialize a new SSLOptions that will use the specified certificate.
    explicit SSLOptions(ServerCertificate cert);
    explicit SSLOptions(base::FilePath cert);
    SSLOptions(const SSLOptions& other);
    ~SSLOptions();

    // Returns the relative filename of the file that contains the
    // |server_certificate|.
    base::FilePath GetCertificateFile() const;

    // The certificate to use when serving requests.
    ServerCertificate server_certificate = CERT_OK;
    base::FilePath custom_certificate;

    // True if a CertificateRequest should be sent to the client during
    // handshaking.
    bool request_client_certificate = false;

    // If |request_client_certificate| is true, an optional list of files,
    // each containing a single, PEM-encoded X.509 certificates. The subject
    // from each certificate will be added to the certificate_authorities
    // field of the CertificateRequest.
    std::vector<base::FilePath> client_authorities;
  };

  // Initialize a TestServer.
  explicit BaseTestServer(Type type);

  // Initialize a TestServer with a specific set of SSLOptions for HTTPS or WSS.
  BaseTestServer(Type type, const SSLOptions& ssl_options);

  BaseTestServer(const BaseTestServer&) = delete;
  BaseTestServer& operator=(const BaseTestServer&) = delete;

  // Starts the server blocking until the server is ready.
  [[nodiscard]] bool Start();

  // Start the test server without blocking. Use this if you need multiple test
  // servers (such as WebSockets and HTTP, or HTTP and HTTPS). You must call
  // BlockUntilStarted on all servers your test requires before executing the
  // test. For example:
  //
  //   // Start the servers in parallel.
  //   ASSERT_TRUE(http_server.StartInBackground());
  //   ASSERT_TRUE(websocket_server.StartInBackground());
  //   // Wait for both servers to be ready.
  //   ASSERT_TRUE(http_server.BlockUntilStarted());
  //   ASSERT_TRUE(websocket_server.BlockUntilStarted());
  //   RunMyTest();
  //
  // Returns true on success.
  [[nodiscard]] virtual bool StartInBackground() = 0;

  // Block until the test server is ready. Returns true on success. See
  // StartInBackground() documentation for more information.
  [[nodiscard]] virtual bool BlockUntilStarted() = 0;

  // Returns the host port pair used by current Python based test server only
  // if the server is started.
  const HostPortPair& host_port_pair() const;

  const base::FilePath& document_root() const { return document_root_; }
  std::string GetScheme() const;
  [[nodiscard]] bool GetAddressList(AddressList* address_list) const;

  GURL GetURL(const std::string& path) const;
  GURL GetURL(const std::string& hostname,
              const std::string& relative_url) const;

  GURL GetURLWithUser(const std::string& path,
                      const std::string& user) const;

  GURL GetURLWithUserAndPassword(const std::string& path,
                                 const std::string& user,
                                 const std::string& password) const;

  static bool GetFilePathWithReplacements(
      const std::string& original_path,
      const std::vector<StringPair>& text_to_replace,
      std::string* replacement_path);

  static bool UsingSSL(Type type) { return type == BaseTestServer::TYPE_WSS; }

  // Enable HTTP basic authentication. Currently this only works for TYPE_WS and
  // TYPE_WSS.
  void set_websocket_basic_auth(bool ws_basic_auth) {
    ws_basic_auth_ = ws_basic_auth;
  }

  // Redirect proxied CONNECT requests to localhost.
  void set_redirect_connect_to_localhost(bool redirect_connect_to_localhost) {
    redirect_connect_to_localhost_ = redirect_connect_to_localhost;
  }

  // Registers the test server's certs for the current process.
  [[nodiscard]] static ScopedTestRoot RegisterTestCerts();

  // Marks the root certificate of an HTTPS test server as trusted for
  // the duration of tests.
  [[nodiscard]] bool LoadTestRootCert();

  // Returns the certificate that the server is using.
  scoped_refptr<X509Certificate> GetCertificate() const;

 protected:
  virtual ~BaseTestServer();
  Type type() const { return type_; }
  const SSLOptions& ssl_options() const { return ssl_options_; }

  bool started() const { return started_; }

  // Gets port currently assigned to host_port_pair_ without checking
  // whether it's available (server started) or not.
  uint16_t GetPort();

  // Sets |port| as the actual port used by Python based test server.
  void SetPort(uint16_t port);

  // Set up internal status when the server is started.
  [[nodiscard]] bool SetupWhenServerStarted();

  // Clean up internal status when starting to stop server.
  void CleanUpWhenStoppingServer();

  // Set path of test resources.
  void SetResourcePath(const base::FilePath& document_root,
                       const base::FilePath& certificates_dir);

  // Parses the server data read from the test server and sets |server_data_|.
  // *port is set to the port number specified in server_data. The port may be
  // different from the local port set in |host_port_pair_|, specifically when
  // using RemoteTestServer (which proxies connections from 127.0.0.1 to a
  // different IP). Returns true on success.
  [[nodiscard]] bool SetAndParseServerData(const std::string& server_data,
                                           int* port);

  // Returns a base::Value::Dict with the arguments for launching the external
  // Python test server, in the form of
  // { argument-name: argument-value, ... }
  //
  // Returns nullopt if an invalid configuration is specified.
  std::optional<base::Value::Dict> GenerateArguments() const;

 private:
  void Init(const std::string& host);

  // Document root of the test server.
  base::FilePath document_root_;

  // Directory that contains the SSL certificates.
  base::FilePath certificates_dir_;

  ScopedTestRoot scoped_test_root_;

  // Address on which the tests should connect to the server. With
  // RemoteTestServer it may be different from the address on which the server
  // listens on.
  HostPortPair host_port_pair_;

  // If |UsingSSL(type_)|, the TLS settings to use for the test server.
  SSLOptions ssl_options_;

  Type type_;

  // Has the server been started?
  bool started_ = false;

  // Enables logging of the server to the console.
  bool log_to_console_ = false;

  // Is WebSocket basic HTTP authentication enabled?
  bool ws_basic_auth_ = false;

  // Redirect proxied CONNECT requests to localhost?
  bool redirect_connect_to_localhost_ = false;

  std::unique_ptr<ScopedPortException> allowed_port_;
};

}  // namespace net

#endif  // NET_TEST_SPAWNED_TEST_SERVER_BASE_TEST_SERVER_H_
