// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERTIFICATE_CHAIN_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERTIFICATE_CHAIN_H_

#include <memory>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/spki_hash_set.h"

namespace base {
class CommandLine;
}  // namespace base

namespace net {
class X509Certificate;
}  // namespace net

namespace content {

class SignedExchangeDevToolsProxy;

// SignedExchangeCertificateChain contains all information in signed exchange
// certificate chain.
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cert-chain-format
class CONTENT_EXPORT SignedExchangeCertificateChain {
 public:
  // An utility class which holds a set of certificates whose errors should be
  // ignored. It parses a comma-delimited list of base64-encoded SHA-256 SPKI
  // fingerprints, and can query if a certificate is included in the set.
  // CONTENT_EXPORT since it is used from the unit test.
  class CONTENT_EXPORT IgnoreErrorsSPKIList {
   public:
    static bool ShouldIgnoreErrors(
        scoped_refptr<net::X509Certificate> certificate);

    explicit IgnoreErrorsSPKIList(const base::CommandLine& command_line);

    IgnoreErrorsSPKIList(const IgnoreErrorsSPKIList&) = delete;
    IgnoreErrorsSPKIList& operator=(const IgnoreErrorsSPKIList&) = delete;

    ~IgnoreErrorsSPKIList();

    // Used for tests to override the instance. Returns the old instance, which
    // should be restored when the test's done.
    static std::unique_ptr<IgnoreErrorsSPKIList> SetInstanceForTesting(
        std::unique_ptr<IgnoreErrorsSPKIList> p);

   private:
    FRIEND_TEST_ALL_PREFIXES(SignedExchangeCertificateChainTest,
                             IgnoreErrorsSPKIList);

    static std::unique_ptr<IgnoreErrorsSPKIList>& GetInstance();

    explicit IgnoreErrorsSPKIList(const std::string& spki_list);
    void Parse(const std::string& spki_list);
    bool ShouldIgnoreErrorsInternal(
        scoped_refptr<net::X509Certificate> certificate);

    network::SPKIHashSet hash_set_;
  };

  static std::unique_ptr<SignedExchangeCertificateChain> Parse(
      base::span<const uint8_t> cert_response_body,
      SignedExchangeDevToolsProxy* devtools_proxy);

  // Regular consumers should use the static Parse() rather than directly
  // calling this.
  SignedExchangeCertificateChain(scoped_refptr<net::X509Certificate> cert,
                                 const std::string& ocsp,
                                 const std::string& sct);
  ~SignedExchangeCertificateChain();

  const scoped_refptr<net::X509Certificate>& cert() const { return cert_; }
  const std::string& ocsp() const { return ocsp_; }
  const std::string& sct() const { return sct_; }

  // Returns true if SPKI hash of |cert_| is included in the
  // --ignore-certificate-errors-spki-list command line flag, and
  // ContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() returns true.
  bool ShouldIgnoreErrors() const;

 private:
  scoped_refptr<net::X509Certificate> cert_;

  std::string ocsp_;
  std::string sct_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CERTIFICATE_CHAIN_H_
