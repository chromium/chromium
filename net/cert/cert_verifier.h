// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFIER_H_
#define NET_CERT_CERT_VERIFIER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list_types.h"
#include "net/base/completion_once_callback.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/x509_certificate.h"

namespace net {

class CertVerifyResult;
class CertVerifierWithUpdatableProc;
class NetLogWithSource;

// CertVerifier represents a service for verifying certificates.
//
// CertVerifiers can handle multiple requests at a time.
class NET_EXPORT CertVerifier {
 public:
  class NET_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when the certificate verifier changes internal configuration.
    // Observers can use this method to invalidate caches that incorporate
    // previous trust decisions.
    //
    // This method will not be called on `CertVerifier::SetConfig`. It is
    // assumed that callers will know to clear their caches when calling the
    // function. https://crbug.com/1427326 tracks migrating `SetConfig` to this
    // mechanism.
    virtual void OnCertVerifierChanged() = 0;
  };

  struct NET_EXPORT Config {
    Config();
    Config(const Config&);
    Config(Config&&);
    ~Config();
    Config& operator=(const Config&);
    Config& operator=(Config&&);

    // Enable online revocation checking via CRLs and OCSP for the certificate
    // chain. Note that revocation checking is soft-fail.
    bool enable_rev_checking = false;

    // Enable online revocation checking via CRLs and OCSP for the certificate
    // chain if the constructed chain terminates in a locally-installed,
    // non-public trust anchor. A revocation error, such as a failure to
    // obtain fresh revocation information, is treated as a hard failure.
    bool require_rev_checking_local_anchors = false;

    // Enable support for SHA-1 signatures if the constructed chain terminates
    // in a locally-installed, non-public trust anchor.
    bool enable_sha1_local_anchors = false;

    // Disable enforcement of the policies described at
    // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
    bool disable_symantec_enforcement = false;
  };

  class Request {
   public:
    Request() = default;

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    // Destruction of the Request cancels it.
    virtual ~Request() = default;
  };

  enum VerifyFlags {
    // If set, actively overrides the current CertVerifier::Config to disable
    // dependent network fetches. This can be used to avoid triggering
    // re-entrancy in the network stack. For example, fetching a PAC script
    // over HTTPS may cause AIA, OCSP, or CRL fetches to block on retrieving
    // the PAC script, while the PAC script fetch is waiting for those
    // dependent fetches, creating a deadlock. When set, this flag prevents
    // those fetches from being started (best effort).
    // Note that cached information may still be used, if it can be accessed
    // without accessing the network.
    VERIFY_DISABLE_NETWORK_FETCHES = 1 << 0,

    VERIFY_FLAGS_LAST = VERIFY_DISABLE_NETWORK_FETCHES
  };

  // Parameters to verify |certificate| against the supplied
  // |hostname| as an SSL server.
  //
  // |hostname| should be a canonicalized hostname (in A-Label form) or IP
  // address in string form, following the rules of a URL host portion. In
  // the case of |hostname| being a domain name, it may contain a trailing
  // dot (e.g. "example.com."), as used to signal to DNS not to perform
  // suffix search, and it will safely be ignored. If |hostname| is an IPv6
  // address, it MUST be in URL form - that is, surrounded in square
  // brackets, such as "[::1]".
  //
  // |flags| is a bitwise OR of VerifyFlags.
  //
  // |ocsp_response| is optional, but if non-empty, should contain an OCSP
  // response obtained via OCSP stapling. It may be ignored by the
  // CertVerifier.
  //
  // |sct_list| is optional, but if non-empty, should contain a
  // SignedCertificateTimestampList from the TLS extension as described in
  // RFC6962 section 3.3.1. It may be ignored by the CertVerifier.
  class NET_EXPORT RequestParams {
   public:
    RequestParams();
    RequestParams(scoped_refptr<X509Certificate> certificate,
                  std::string_view hostname,
                  int flags,
                  std::string_view ocsp_response,
                  std::string_view sct_list);
    RequestParams(const RequestParams& other);
    ~RequestParams();

    const scoped_refptr<X509Certificate>& certificate() const {
      return certificate_;
    }
    const std::string& hostname() const { return hostname_; }
    int flags() const { return flags_; }
    const std::string& ocsp_response() const { return ocsp_response_; }
    const std::string& sct_list() const { return sct_list_; }

    bool operator==(const RequestParams& other) const;
    bool operator<(const RequestParams& other) const;

   private:
    scoped_refptr<X509Certificate> certificate_;
    std::string hostname_;
    int flags_;
    std::string ocsp_response_;
    std::string sct_list_;

    // Used to optimize sorting/indexing comparisons.
    std::string key_;
  };

  // When the verifier is destroyed, all certificate verification requests are
  // canceled, and their completion callbacks will not be called.
  virtual ~CertVerifier() = default;

  // Verifies the given certificate against the given hostname as an SSL server.
  // Returns OK if successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask and |verify_result->verified_cert|, is always filled out regardless
  // of the return value. If the certificate has multiple errors, the
  // corresponding status flags are set in |verify_result->cert_status|, and the
  // error code for the most serious error is returned.
  //
  // |callback| must not be null. ERR_IO_PENDING is returned if the operation
  // could not be completed synchronously, in which case the result code will
  // be passed to the callback when available.
  //
  // |*out_req| is used to store a request handle in the event of asynchronous
  // completion (when Verify returns ERR_IO_PENDING). Provided that neither
  // the CertVerifier nor the Request have been deleted, |callback| will be
  // invoked once the underlying verification finishes. If either the
  // CertVerifier or the Request are deleted, then |callback| will be Reset()
  // and will not be invoked. It is fine for |out_req| to outlive the
  // CertVerifier, and it is fine to reset |out_req| or delete the
  // CertVerifier during the processing of |callback|.
  //
  // If Verify() completes synchronously then |out_req| *may* be reset to
  // nullptr. However it is not guaranteed that all implementations will reset
  // it in this case.
  virtual int Verify(const RequestParams& params,
                     CertVerifyResult* verify_result,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* out_req,
                     const NetLogWithSource& net_log) = 0;

  // Sets the configuration for new certificate verifications to be |config|.
  // Any in-progress verifications (i.e. those with outstanding Request
  // handles) will continue using the old configuration. This may be called
  // throughout the CertVerifier's lifetime in response to configuration
  // changes from embedders.
  // Note: As configuration changes will replace any existing configuration,
  // this should only be called by the logical 'owner' of this CertVerifier.
  // Callers should NOT attempt to change configuration for single calls, and
  // should NOT attempt to change configuration for CertVerifiers they do not
  // explicitly manage.
  virtual void SetConfig(const Config& config) = 0;

  // Add an observer to be notified when the CertVerifier has changed.
  // RemoveObserver() must be called before |observer| is destroyed.
  virtual void AddObserver(Observer* observer) = 0;

  // Remove an observer added with AddObserver().
  virtual void RemoveObserver(Observer* observer) = 0;

  // Creates a CertVerifier implementation that verifies certificates using
  // the preferred underlying cryptographic libraries.  |cert_net_fetcher| may
  // not be used, depending on the platform.
  static std::unique_ptr<CertVerifierWithUpdatableProc>
  CreateDefaultWithoutCaching(scoped_refptr<CertNetFetcher> cert_net_fetcher);

  // Wraps the result of |CreateDefaultWithoutCaching| in a CachingCertVerifier
  // and a CoalescingCertVerifier.
  static std::unique_ptr<CertVerifier> CreateDefault(
      scoped_refptr<CertNetFetcher> cert_net_fetcher);
};

// Overloads for comparing two configurations. Note, comparison is shallow -
// that is, two scoped_refptr<CRLSet>s are equal iff they point to the same
// object.
NET_EXPORT bool operator==(const CertVerifier::Config& lhs,
                           const CertVerifier::Config& rhs);
NET_EXPORT bool operator!=(const CertVerifier::Config& lhs,
                           const CertVerifier::Config& rhs);

// A CertVerifier that can update its CertVerifyProc while it is running.
class NET_EXPORT CertVerifierWithUpdatableProc : public CertVerifier {
 public:
  // Update the CertVerifyProc with a new set of parameters.
  virtual void UpdateVerifyProcData(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      const net::CertVerifyProc::ImplParams& impl_params,
      const net::CertVerifyProc::InstanceParams& instance_params) = 0;
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFIER_H_
