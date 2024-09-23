// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/types/expected.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/socket/ssl_server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_builder.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "third_party/boringssl/src/pki/ocsp_revocation_status.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class StreamSocket;
class TCPServerSocket;

namespace test_server {

class EmbeddedTestServerConnectionListener;
class HttpConnection;
class HttpResponse;
class HttpResponseDelegate;
struct HttpRequest;

class EmbeddedTestServer;

// Enum representing the possible outcomes of handling an upgrade request.
// - kUpgraded: The request was successfully upgraded to a WebSocket connection.
// - kNotHandled: The request was not handled as an upgrade and should be
// processed as a normal HTTP request.
enum class UpgradeResult {
  kUpgraded,
  kNotHandled,
};

// Returned by the Start[AcceptingConnections]WithHandle() APIs, to simplify
// correct shutdown ordering of the EmbeddedTestServer. Shutdown() is invoked
// on the associated test server when the handle goes out of scope. The handle
// must therefore be destroyed before the test server.
class EmbeddedTestServerHandle {
 public:
  EmbeddedTestServerHandle() = default;
  EmbeddedTestServerHandle(EmbeddedTestServerHandle&& other);
  EmbeddedTestServerHandle& operator=(EmbeddedTestServerHandle&& other);
  ~EmbeddedTestServerHandle();

  bool is_valid() const { return test_server_; }
  explicit operator bool() const { return test_server_; }

 private:
  friend class EmbeddedTestServer;

  explicit EmbeddedTestServerHandle(EmbeddedTestServer* test_server);
  raw_ptr<EmbeddedTestServer> test_server_ = nullptr;
};

// Class providing an HTTP server for testing purpose. This is a basic server
// providing only an essential subset of HTTP/1.1 protocol. Especially,
// it assumes that the request syntax is correct. It *does not* support
// a Chunked Transfer Encoding.
//
// The common use case for unit tests is below:
//
// void SetUp() {
//   test_server_ = std::make_unique<EmbeddedTestServer>();
//   test_server_->RegisterRequestHandler(
//       base::BindRepeating(&FooTest::HandleRequest, base::Unretained(this)));
//   ASSERT_TRUE((test_server_handle_ = test_server_.StartAndReturnHandle()));
// }
//
// std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
//   GURL absolute_url = test_server_->GetURL(request.relative_url);
//   if (absolute_url.path() != "/test")
//     return nullptr;
//
//   auto http_response = std::make_unique<BasicHttpResponse>();
//   http_response->set_code(net::HTTP_OK);
//   http_response->set_content("hello");
//   http_response->set_content_type("text/plain");
//   return http_response;
// }
//
// For a test that spawns another process such as browser_tests, it is
// suggested to call Start in SetUpOnMainThread after the process is spawned.
//  If you have to do it before the process spawns, you need to first setup the
// listen socket so that there is no no other threads running while spawning
// the process. To do so, please follow the following example:
//
// void SetUp() {
//   ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
//   ...
//   InProcessBrowserTest::SetUp();
// }
//
// void SetUpOnMainThread() {
//   // Starts the accept IO thread.
//   embedded_test_server()->StartAcceptingConnections();
// }
//
class EmbeddedTestServer {
 public:
  enum Type {
    TYPE_HTTP,
    TYPE_HTTPS,
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net.test
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

    // A certificate that only contains a commonName, rather than also
    // including a subjectAltName extension.
    CERT_COMMON_NAME_ONLY,

    // A certificate that is a leaf certificate signed with SHA-1.
    CERT_SHA1_LEAF,

    // A certificate that is signed by an intermediate certificate.
    CERT_OK_BY_INTERMEDIATE,

    // A certificate with invalid notBefore and notAfter times. Windows'
    // certificate library will not parse this certificate.
    CERT_BAD_VALIDITY,

    // A certificate that covers a number of test names. See [test_names] in
    // net/data/ssl/scripts/ee.cnf. More may be added by editing this list and
    // and rerunning net/data/ssl/scripts/generate-test-certs.sh.
    CERT_TEST_NAMES,

    // An RSA certificate with the keyUsage extension specifying that the key
    // is only for encipherment.
    CERT_KEY_USAGE_RSA_ENCIPHERMENT,

    // An RSA certificate with the keyUsage extension specifying that the key
    // is only for digital signatures.
    CERT_KEY_USAGE_RSA_DIGITAL_SIGNATURE,

    // A certificate will be generated at runtime. A ServerCertificateConfig
    // passed to SetSSLConfig may be used to configure the details of the
    // generated certificate.
    CERT_AUTO,
  };

  enum class RootType {
    // The standard test_root_ca.pem certificate will be used, which should be
    // trusted by default. (See `RegisterTestCerts`.)
    kTestRootCa,
    // A new CA certificate will be generated at runtime. The generated
    // certificate chain will not be trusted unless the test itself trusts it.
    kUniqueRoot,
  };

  enum class IntermediateType {
    // Generated cert is issued directly by the CA.
    kNone,
    // Generated cert is issued by a generated intermediate cert, which is
    // included in the TLS handshake.
    kInHandshake,
    // Generated cert is issued by a generated intermediate, which is NOT
    // included in the TLS handshake, but is available through the leaf's
    // AIA caIssuers URL.
    kByAIA,
    // Generated cert is issued by a generated intermediate, which is NOT
    // included in the TLS handshake and not served by an AIA server.
    kMissing,
  };

  struct OCSPConfig {
    // Enumerates the types of OCSP response that the testserver can produce.
    enum class ResponseType {
      // OCSP will not be enabled for the corresponding config.
      kOff,
      // These correspond to the OCSPResponseStatus enumeration in RFC
      // 6960.
      kSuccessful,
      kMalformedRequest,
      kInternalError,
      kTryLater,
      kSigRequired,
      kUnauthorized,
      // The response will not be valid bssl::OCSPResponse DER.
      kInvalidResponse,
      // bssl::OCSPResponse will be valid DER but the contained ResponseData
      // will not.
      kInvalidResponseData,
    };

    // OCSPProduced describes the time of the producedAt field in the
    // OCSP response relative to the certificate the response is for.
    enum class Produced {
      // producedAt is between certificate's notBefore and notAfter dates.
      kValid,
      // producedAt is before certificate's notBefore date.
      kBeforeCert,
      // producedAt is after certificate's notAfter date.
      kAfterCert,
    };

    struct SingleResponse {
      // Date describes the thisUpdate..nextUpdate ranges for OCSP
      // singleResponses, relative to the current time.
      enum class Date {
        // The singleResponse is valid for 7 days, and includes the current
        // time.
        kValid,
        // The singleResponse is valid for 7 days, but nextUpdate is before the
        // current time.
        kOld,
        // The singleResponse is valid for 7 days, but thisUpdate is after the
        // current time.
        kEarly,
        // The singleResponse is valid for 366 days, and includes the current
        // time.
        kLong,
        // The singleResponse is valid for 368 days, and includes the current
        // time.
        kLonger,
      };

      // Configures whether a generated OCSP singleResponse's serial field
      // matches the serial number of the target certificate.
      enum class Serial {
        kMatch,
        kMismatch,
      };

      bssl::OCSPRevocationStatus cert_status = bssl::OCSPRevocationStatus::GOOD;
      Date ocsp_date = Date::kValid;
      Serial serial = Serial::kMatch;
    };

    OCSPConfig();
    // Configure OCSP response with |response_type|.
    explicit OCSPConfig(ResponseType response_type);
    // Configure a successful OCSP response with |single_responses|. |produced|
    // specifies the response's producedAt value, relative to the validity
    // period of the certificate the OCSPConfig is for.
    explicit OCSPConfig(std::vector<SingleResponse> single_responses,
                        Produced produced = Produced::kValid);
    OCSPConfig(const OCSPConfig&);
    OCSPConfig(OCSPConfig&&);
    ~OCSPConfig();
    OCSPConfig& operator=(const OCSPConfig&);
    OCSPConfig& operator=(OCSPConfig&&);

    ResponseType response_type = ResponseType::kOff;
    Produced produced = Produced::kValid;
    std::vector<SingleResponse> single_responses;
  };

  // Configuration for generated server certificate.
  struct ServerCertificateConfig {
    ServerCertificateConfig();
    ServerCertificateConfig(const ServerCertificateConfig&);
    ServerCertificateConfig(ServerCertificateConfig&&);
    ~ServerCertificateConfig();
    ServerCertificateConfig& operator=(const ServerCertificateConfig&);
    ServerCertificateConfig& operator=(ServerCertificateConfig&&);

    // Configure what root CA certificate should be used to issue the generated
    // certificate chain.
    RootType root = RootType::kTestRootCa;

    // Configure whether the generated certificate chain should include an
    // intermediate, and if so, how it is delivered to the client.
    IntermediateType intermediate = IntermediateType::kNone;

    // Configure OCSP handling.
    // Note: In the current implementation the AIA request handler does not
    // actually parse the OCSP request (a different OCSP URL is used for each
    // cert). So this is useful for testing the client's handling of the OCSP
    // response, but not for testing that the client is sending a proper OCSP
    // request.
    //
    // AIA OCSP for the leaf cert. If |kOff|, no AIA OCSP URL will be included
    // in the leaf cert.
    OCSPConfig ocsp_config;
    // Stapled OCSP for the leaf cert. If |kOff|, OCSP Stapling will not be
    // used.
    OCSPConfig stapled_ocsp_config;
    // AIA OCSP for the intermediate cert. If |kOff|, no AIA OCSP URL will be
    // included in the intermediate cert. It is invalid to supply a
    // configuration other than |kOff| if |intermediate| is |kNone|.
    OCSPConfig intermediate_ocsp_config;

    // Certificate policy OIDs, in text notation (e.g. "1.2.3.4"). If
    // non-empty, the policies will be added to the leaf cert and the
    // intermediate cert (if an intermediate is configured).
    std::vector<std::string> policy_oids;

    // A list of DNS names to include in the leaf subjectAltName extension.
    std::vector<std::string> dns_names;

    // A list of IP addresses to include in the leaf subjectAltName extension.
    std::vector<net::IPAddress> ip_addresses;

    // A list of key usages to include in the leaf keyUsage extension.
    std::vector<bssl::KeyUsageBit> key_usages;

    // Generate embedded SCTList in the certificate for the specified logs.
    std::vector<CertBuilder::SctConfig> embedded_scts;
  };

  using UpgradeResultOrHttpResponse =
      base::expected<UpgradeResult, std::unique_ptr<HttpResponse>>;
  using HandleUpgradeRequestCallback =
      base::RepeatingCallback<UpgradeResultOrHttpResponse(
          const HttpRequest& request,
          HttpConnection* connection)>;
  typedef base::RepeatingCallback<std::unique_ptr<HttpResponse>(
      const HttpRequest& request)>
      HandleRequestCallback;
  typedef base::RepeatingCallback<void(const HttpRequest& request)>
      MonitorRequestCallback;

  // Creates a http test server. StartAndReturnHandle() must be called to start
  // the server.
  // |type| indicates the protocol type of the server (HTTP/HTTPS).
  //
  //  When a TYPE_HTTPS server is created, EmbeddedTestServer will call
  // EmbeddedTestServer::RegisterTestCerts(), so that when the default
  // CertVerifiers are run in-process, they will recognize the test server's
  // certs. However, if the test server is running in a different process from
  // the CertVerifiers, EmbeddedTestServer::RegisterTestCerts() must be called
  // in any process where CertVerifiers are expected to accept the
  // EmbeddedTestServer's certs.
  EmbeddedTestServer();
  explicit EmbeddedTestServer(
      Type type,
      HttpConnection::Protocol protocol = HttpConnection::Protocol::kHttp1);
  ~EmbeddedTestServer();

  //  Send a request to the server to be handled. If a response is created,
  //  SendResponseBytes() should be called on the provided HttpConnection.
  void HandleRequest(base::WeakPtr<HttpResponseDelegate> delegate,
                     std::unique_ptr<HttpRequest> request,
                     const StreamSocket* socket);

  // Notify the server that a connection is no longer usable and is safe to
  // destroy. For H/1 connections, this means a single request/response
  // interaction, as keep-alive connections are not supported. If the
  // connection listener is present and the socket is still connected, the
  // listener will be notified.
  void RemoveConnection(
      HttpConnection* connection,
      EmbeddedTestServerConnectionListener* listener = nullptr);

  // Registers the EmbeddedTestServer's certs for the current process. See
  // constructor documentation for more information.
  [[nodiscard]] static ScopedTestRoot RegisterTestCerts();

  // Sets a connection listener, that would be notified when various connection
  // events happen. May only be called before the server is started. Caller
  // maintains ownership of the listener.
  void SetConnectionListener(EmbeddedTestServerConnectionListener* listener);

  // Initializes and waits until the server is ready to accept requests.
  // This is the equivalent of calling InitializeAndListen() followed by
  // StartAcceptingConnectionsAndReturnHandle().
  // Returns a "handle" which will ShutdownAndWaitUntilComplete() when
  // destroyed, or null if the listening socket could not be created.
  [[nodiscard]] EmbeddedTestServerHandle StartAndReturnHandle(int port = 0);

  // Equivalent of StartAndReturnHandle(), but requires manual Shutdown() by
  // the caller.
  [[nodiscard]] bool Start(int port = 0,
                           std::string_view address = "127.0.0.1");

  // Starts listening for incoming connections but will not yet accept them.
  // Returns whether a listening socket has been successfully created.
  [[nodiscard]] bool InitializeAndListen(
      int port = 0,
      std::string_view address = "127.0.0.1");

  // Starts the Accept IO Thread and begins accepting connections.
  [[nodiscard]] EmbeddedTestServerHandle
  StartAcceptingConnectionsAndReturnHandle();

  // Equivalent of StartAcceptingConnectionsAndReturnHandle(), but requires
  // manual Shutdown() by the caller.
  void StartAcceptingConnections();

  // Shuts down the http server and waits until the shutdown is complete.
  // Prefer to use the Start*AndReturnHandle() APIs to manage shutdown, if
  // possible.
  [[nodiscard]] bool ShutdownAndWaitUntilComplete();

  // Checks if the server has started listening for incoming connections.
  bool Started() const { return listen_socket_.get() != nullptr; }

  static base::FilePath GetRootCertPemPath();

  HostPortPair host_port_pair() const {
    return HostPortPair::FromURL(base_url_);
  }

  // Returns the base URL to the server, which looks like
  // http://127.0.0.1:<port>/, where <port> is the actual port number used by
  // the server.
  const GURL& base_url() const { return base_url_; }

  // Returns a URL to the server based on the given relative URL, which
  // should start with '/'. For example: GetURL("/path?query=foo") =>
  // http://127.0.0.1:<port>/path?query=foo.
  GURL GetURL(std::string_view relative_url) const;

  // Similar to the above method with the difference that it uses the supplied
  // |hostname| for the URL instead of 127.0.0.1. The hostname should be
  // resolved to 127.0.0.1.
  GURL GetURL(std::string_view hostname, std::string_view relative_url) const;

  // Convenience function equivalent to calling url::Origin::Create(base_url()).
  // Will use the GetURL() variant that takes a hostname as the base URL, if
  // `hostname` is non-null.
  url::Origin GetOrigin(
      const std::optional<std::string>& hostname = std::nullopt) const;

  // Returns the address list needed to connect to the server.
  [[nodiscard]] bool GetAddressList(AddressList* address_list) const;

  // Returns the IP Address to connect to the server as a string.
  std::string GetIPLiteralString() const;

  // Returns the port number used by the server.
  uint16_t port() const { return port_; }

  // SetSSLConfig sets the SSL configuration for the server. It is invalid to
  // call after the server is started. If called multiple times, the last call
  // will have effect.
  void SetSSLConfig(ServerCertificate cert, const SSLServerConfig& ssl_config);
  void SetSSLConfig(ServerCertificate cert);
  void SetSSLConfig(const ServerCertificateConfig& cert_config,
                    const SSLServerConfig& ssl_config);
  void SetSSLConfig(const ServerCertificateConfig& cert_config);

  // TODO(mattm): make this [[nodiscard]]
  bool ResetSSLConfig(ServerCertificate cert,
                      const SSLServerConfig& ssl_config);

  // Configures the test server to generate a certificate that covers the
  // specified hostnames. This implicitly also includes 127.0.0.1 in the
  // certificate. It is invalid to call after the server is started. If called
  // multiple times, the last call will have effect.
  // Convenience method for configuring an HTTPS test server when a test needs
  // to support a set of hostnames over HTTPS, rather than explicitly setting
  /// up a full config using SetSSLConfig().
  void SetCertHostnames(std::vector<std::string> hostnames);

  // Returns the certificate that the server is using.
  // If using a generated ServerCertificate type, this must not be called before
  // InitializeAndListen() has been called.
  scoped_refptr<X509Certificate> GetCertificate();

  // Returns any generated intermediates that the server may be using. May
  // return null if no intermediate is generated.  Must not be called before
  // InitializeAndListen().
  scoped_refptr<X509Certificate> GetGeneratedIntermediate();

  // Returns the root certificate that issued the certificate the server is
  // using.  Must not be called before InitializeAndListen().
  scoped_refptr<X509Certificate> GetRoot();

  // Registers request handler which serves files from |directory|.
  // For instance, a request to "/foo.html" is served by "foo.html" under
  // |directory|. Files under sub directories are also handled in the same way
  // (i.e. "/foo/bar.html" is served by "foo/bar.html" under |directory|).
  // TODO(svaldez): Merge ServeFilesFromDirectory and
  // ServeFilesFromSourceDirectory.
  void ServeFilesFromDirectory(const base::FilePath& directory);

  // Serves files relative to DIR_SRC_TEST_DATA_ROOT.
  void ServeFilesFromSourceDirectory(std::string_view relative);
  void ServeFilesFromSourceDirectory(const base::FilePath& relative);

  // Registers the default handlers and serve additional files from the
  // |directory| directory, relative to DIR_SRC_TEST_DATA_ROOT.
  void AddDefaultHandlers(const base::FilePath& directory);

  // Returns the directory that files will be served from if |relative| is
  // passed to ServeFilesFromSourceDirectory().
  static base::FilePath GetFullPathFromSourceDirectory(
      const base::FilePath& relative);

  // Adds all default handlers except, without serving additional files from any
  // directory.
  void AddDefaultHandlers();

  // Adds a handler callback to process WebSocket upgrade requests.
  // |callback| will be invoked on the server's IO thread when a request
  // attempts to upgrade to a WebSocket connection. Note that:
  // 1. All upgrade request handlers must be registered before the server is
  //    Start()ed.
  // 2. This method is not supported for HTTP/2 connections.
  // 3. The server should be Shutdown() before any variables referred to by
  //    |callback| (e.g., via base::Unretained(&local)) are deleted. Using the
  //    Start*WithHandle() API variants is recommended for this reason.
  void RegisterUpgradeRequestHandler(
      const HandleUpgradeRequestCallback& callback);

  // Adds a request handler that can perform any general-purpose processing.
  // |callback| will be invoked on the server's IO thread. Note that:
  // 1. All handlers must be registered before the server is Start()ed.
  // 2. The server should be Shutdown() before any variables referred to by
  //    |callback| (e.g. via base::Unretained(&local)) are deleted. Using the
  //    Start*WithHandle() API variants is recommended for this reason.
  void RegisterRequestHandler(const HandleRequestCallback& callback);

  // Adds a request monitor that will be called before any handlers. Monitors
  // can be used to observe requests, but not to respond to them.
  // See RegisterRequestHandler() for notes on usage.
  void RegisterRequestMonitor(const MonitorRequestCallback& callback);

  // Adds a default request handler, to be called if no user-specified handler
  // handles the request.
  // See RegisterRequestHandler() for notes on usage.
  void RegisterDefaultHandler(const HandleRequestCallback& callback);

  bool FlushAllSocketsAndConnectionsOnUIThread();
  void FlushAllSocketsAndConnections();

  // Adds an origin/accept_ch pair to add to an ACCEPT_CH HTTP/2 frame. If any
  // pairs have been added, the ALPS TLS extension will be populated, which
  // will act as though an ACCEPT_CH frame was sent by the server before the
  // first frame is sent by a client. For more information, see
  // draft-vvv-tls-alps-01 and section 4.1 (HTTP/2 ACCEPT_CH Frame) of
  // draft-davidben-http-client-hint-reliability
  //
  // Only valid before Start() or ResetSSLServerConfig(). Only valid when
  // constructed with PROTOCOL_HTTP2. For the default host, use an empty
  // string.
  void SetAlpsAcceptCH(std::string hostname, std::string accept_ch);

 private:
  // Returns the file name of the certificate the server is using. The test
  // certificates can be found in net/data/ssl/certificates/.
  std::string GetCertificateName() const;

  // Shuts down the server.
  void ShutdownOnIOThread();

  // Sets the SSL configuration for the server. It is invalid for |cert_config|
  // to be non-null if |cert| is not CERT_AUTO.
  void SetSSLConfigInternal(ServerCertificate cert,
                            const ServerCertificateConfig* cert_config,
                            const SSLServerConfig& ssl_config);

  // Resets the SSLServerConfig on the IO thread.
  bool ResetSSLConfigOnIOThread(ServerCertificate cert,
                                const SSLServerConfig& ssl_config);

  HttpConnection* GetConnectionForSocket(const StreamSocket* socket);

  // Upgrade the TCP connection to one over SSL.
  std::unique_ptr<SSLServerSocket> DoSSLUpgrade(
      std::unique_ptr<StreamSocket> connection);
  // Handles async callback when the SSL handshake has been completed.
  void OnHandshakeDone(HttpConnection* http_connection, int rv);
  // Begins new connection if handshake resulted in a connection
  void HandleHandshakeResults();

  // Begins accepting new client connections.
  void DoAcceptLoop();
  // Handles async callback when there is a new client socket. |rv| is the
  // return value of the socket Accept.
  void OnAcceptCompleted(int rv);
  // Adds the new |socket| to the list of clients and begins the reading
  // data.
  void HandleAcceptResult(std::unique_ptr<StreamSocket> socket_ptr);

  // Create a connection with a socket, add it to the map, and return pointers
  // to both.
  HttpConnection* AddConnection(std::unique_ptr<StreamSocket> socket_ptr);

  // Handles async callback when new data has been read from the |connection|.
  void OnReadCompleted(HttpConnection* connection, int rv);

  // Returns true if the current |cert_| configuration uses a static
  // pre-generated cert loaded from the filesystem.
  bool UsingStaticCert() const;

  // Reads server certificate and private key from file. May only be called if
  // |cert_| refers to a file-based cert & key.
  [[nodiscard]] bool InitializeCertAndKeyFromFile();

  // Generate server certificate and private key. May only be called if |cert_|
  // refers to a generated cert & key.
  [[nodiscard]] bool GenerateCertAndKey();

  // Initializes the SSLServerContext so that SSLServerSocket connections may
  // share the same cache
  [[nodiscard]] bool InitializeSSLServerContext();

  // Posts a task to the |io_thread_| and waits for a reply.
  [[nodiscard]] bool PostTaskToIOThreadAndWait(base::OnceClosure closure);

  // Posts a task that returns a true/false success/fail value to the
  // |io_thread_| and waits for a reply.
  [[nodiscard]] bool PostTaskToIOThreadAndWaitWithResult(
      base::OnceCallback<bool()> task);

  const bool is_using_ssl_;
  const HttpConnection::Protocol protocol_;

  std::unique_ptr<base::Thread> io_thread_;

  std::unique_ptr<TCPServerSocket> listen_socket_;
  std::unique_ptr<StreamSocket> accepted_socket_;

  raw_ptr<EmbeddedTestServerConnectionListener, DanglingUntriaged>
      connection_listener_ = nullptr;
  uint16_t port_ = 0;
  GURL base_url_;
  IPEndPoint local_endpoint_;

  std::map<const StreamSocket*, std::unique_ptr<HttpConnection>> connections_;

  // Vector of registered and default request handlers and monitors.
  std::vector<HandleUpgradeRequestCallback> upgrade_request_handlers_;
  std::vector<HandleRequestCallback> request_handlers_;
  std::vector<MonitorRequestCallback> request_monitors_;
  std::vector<HandleRequestCallback> default_request_handlers_;

  base::ThreadChecker thread_checker_;

  ScopedTestRoot scoped_test_root_;
  net::SSLServerConfig ssl_config_;
  ServerCertificate cert_ = CERT_OK;
  ServerCertificateConfig cert_config_;
  scoped_refptr<X509Certificate> x509_cert_;
  // May be null if no intermediate is generated.
  scoped_refptr<X509Certificate> intermediate_;
  scoped_refptr<X509Certificate> root_;
  bssl::UniquePtr<EVP_PKEY> private_key_;
  base::flat_map<std::string, std::string> alps_accept_ch_;
  std::unique_ptr<SSLServerContext> context_;

  // HTTP server that handles AIA URLs that are embedded in this test server's
  // certificate when the server certificate is one of the CERT_AUTO variants.
  std::unique_ptr<EmbeddedTestServer> aia_http_server_;

  base::WeakPtrFactory<EmbeddedTestServer> weak_factory_{this};
};

}  // namespace test_server

// TODO(svaldez): Refactor EmbeddedTestServer to be in the net namespace.
using test_server::EmbeddedTestServer;

}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_H_
