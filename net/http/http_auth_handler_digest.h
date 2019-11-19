// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

namespace net {

// Code for handling http digest authentication.
class NET_EXPORT_PRIVATE HttpAuthHandlerDigest : public HttpAuthHandler {
 public:
  // A NonceGenerator is a simple interface for generating client nonces.
  // Unit tests can override the default client nonce behavior with fixed
  // nonce generation to get reproducible results.
  class NET_EXPORT_PRIVATE NonceGenerator {
   public:
    NonceGenerator();
    virtual ~NonceGenerator();

    // Generates a client nonce.
    virtual std::string GenerateNonce() const = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(NonceGenerator);
  };

  // DynamicNonceGenerator does a random shuffle of 16
  // characters to generate a client nonce.
  class DynamicNonceGenerator : public NonceGenerator {
   public:
    DynamicNonceGenerator();
    std::string GenerateNonce() const override;

   private:
    DISALLOW_COPY_AND_ASSIGN(DynamicNonceGenerator);
  };

  // FixedNonceGenerator always uses the same string specified at
  // construction time as the client nonce.
  class NET_EXPORT_PRIVATE FixedNonceGenerator : public NonceGenerator {
   public:
    explicit FixedNonceGenerator(const std::string& nonce);

    std::string GenerateNonce() const override;

   private:
    const std::string nonce_;
    DISALLOW_COPY_AND_ASSIGN(FixedNonceGenerator);
  };

  class NET_EXPORT_PRIVATE Factory : public HttpAuthHandlerFactory {
   public:
    Factory();
    ~Factory() override;

    // This factory owns the passed in |nonce_generator|.
    void set_nonce_generator(const NonceGenerator* nonce_generator);

    int CreateAuthHandler(HttpAuthChallengeTokenizer* challenge,
                          HttpAuth::Target target,
                          const SSLInfo& ssl_info,
                          const GURL& origin,
                          CreateReason reason,
                          int digest_nonce_count,
                          const NetLogWithSource& net_log,
                          HostResolver* host_resolver,
                          std::unique_ptr<HttpAuthHandler>* handler) override;

   private:
    std::unique_ptr<const NonceGenerator> nonce_generator_;
  };

 private:
  // HttpAuthHandler
  bool Init(HttpAuthChallengeTokenizer* challenge,
            const SSLInfo& ssl_info) override;
  int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                            const HttpRequestInfo* request,
                            CompletionOnceCallback callback,
                            std::string* auth_token) override;
  HttpAuth::AuthorizationResult HandleAnotherChallengeImpl(
      HttpAuthChallengeTokenizer* challenge) override;

  FRIEND_TEST_ALL_PREFIXES(HttpAuthHandlerDigestTest, ParseChallenge);
  FRIEND_TEST_ALL_PREFIXES(HttpAuthHandlerDigestTest, AssembleCredentials);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest, DigestPreAuthNonceCount);

  // Possible values for the "algorithm" property.
  enum DigestAlgorithm {
    // No algorithm was specified. According to RFC 2617 this means
    // we should default to ALGORITHM_MD5.
    ALGORITHM_UNSPECIFIED,

    // Hashes are run for every request.
    ALGORITHM_MD5,

    // Hash is run only once during the first WWW-Authenticate handshake.
    // (SESS means session).
    ALGORITHM_MD5_SESS,
  };

  // Possible values for QualityOfProtection.
  // auth-int is not supported, see http://crbug.com/62890 for justification.
  enum QualityOfProtection {
    QOP_UNSPECIFIED,
    QOP_AUTH,
  };

  // |nonce_count| indicates how many times the server-specified nonce has
  // been used so far.
  // |nonce_generator| is used to create a client nonce, and is not owned by
  // the handler. The lifetime of the |nonce_generator| must exceed that of this
  // handler.
  HttpAuthHandlerDigest(int nonce_count, const NonceGenerator* nonce_generator);
  ~HttpAuthHandlerDigest() override;

  // Parse the challenge, saving the results into this instance.
  // Returns true on success.
  bool ParseChallenge(HttpAuthChallengeTokenizer* challenge);

  // Parse an individual property. Returns true on success.
  bool ParseChallengeProperty(base::StringPiece name, base::StringPiece value);

  // Generates a random string, to be used for client-nonce.
  static std::string GenerateNonce();

  // Convert enum value back to string.
  static std::string QopToString(QualityOfProtection qop);
  static std::string AlgorithmToString(DigestAlgorithm algorithm);

  // Extract the method and path of the request, as needed by
  // the 'A2' production. (path may be a hostname for proxy).
  void GetRequestMethodAndPath(const HttpRequestInfo* request,
                               std::string* method,
                               std::string* path) const;

  // Build up  the 'response' production.
  std::string AssembleResponseDigest(const std::string& method,
                                     const std::string& path,
                                     const AuthCredentials& credentials,
                                     const std::string& cnonce,
                                     const std::string& nc) const;

  // Build up  the value for (Authorization/Proxy-Authorization).
  std::string AssembleCredentials(const std::string& method,
                                  const std::string& path,
                                  const AuthCredentials& credentials,
                                  const std::string& cnonce,
                                  int nonce_count) const;

  // Information parsed from the challenge.
  std::string nonce_;
  std::string domain_;
  std::string opaque_;
  bool stale_;
  DigestAlgorithm algorithm_;
  QualityOfProtection qop_;

  // The realm as initially encoded over-the-wire. This is used in the
  // challenge text, rather than |realm_| which has been converted to
  // UTF-8.
  std::string original_realm_;

  int nonce_count_;
  const NonceGenerator* nonce_generator_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_
