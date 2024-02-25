// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SSL_HOST_STATE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SSL_HOST_STATE_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "net/cert/x509_certificate.h"

class GURL;

namespace content {

class StoragePartition;

// The SSLHostStateDelegate encapsulates the host-specific state for SSL errors.
// For example, SSLHostStateDelegate remembers whether the user has whitelisted
// a particular broken cert for use with particular host.  We separate this
// state from the SSLManager because this state is shared across many navigation
// controllers.
//
// SSLHostStateDelegate may be implemented by the embedder to provide a storage
// strategy for certificate decisions or it may be left unimplemented to use a
// default strategy of not remembering decisions at all.
class SSLHostStateDelegate {
 public:
  // The judgements that can be reached by a user for invalid certificates.
  enum CertJudgment {
    DENIED,
    ALLOWED
  };

  // The types of nonsecure subresources that this class keeps track of.
  enum InsecureContentType {
    // A  MIXED subresource was loaded over HTTP on an HTTPS page.
    MIXED_CONTENT,
    // A CERT_ERRORS subresource was loaded over HTTPS with certificate
    // errors on an HTTPS page.
    CERT_ERRORS_CONTENT,
  };

  // Records that |cert| is permitted to be used for |host| in the future, for
  // a specified |error| type.
  virtual void AllowCert(const std::string&,
                         const net::X509Certificate& cert,
                         int error,
                         StoragePartition* storage_partition) = 0;

  // Clear allow preferences matched by |host_filter|. If the filter is null,
  // clear all preferences.
  virtual void Clear(
      base::RepeatingCallback<bool(const std::string&)> host_filter) = 0;

  // Queries whether |cert| is allowed for |host| and |error|. Returns true in
  virtual CertJudgment QueryPolicy(const std::string& host,
                                   const net::X509Certificate& cert,
                                   int error,
                                   StoragePartition* storage_partition) = 0;

  // Records that a host has run insecure content of the given |content_type|.
  virtual void HostRanInsecureContent(const std::string& host,
                                      int child_id,
                                      InsecureContentType content_type) = 0;

  // Returns whether the specified host ran insecure content of the given
  // |content_type|.
  virtual bool DidHostRunInsecureContent(const std::string& host,
                                         int child_id,
                                         InsecureContentType content_type) = 0;

  // Allowlists site so it can be loaded over HTTP when HTTPS-First Mode is
  // enabled.
  virtual void AllowHttpForHost(const std::string& host,
                                StoragePartition* storage_partition) = 0;

  // Returns whether site is allowed to load over HTTP when HTTPS-First Mode is
  // enabled.
  virtual bool IsHttpAllowedForHost(const std::string& host,
                                    StoragePartition* storage_partition) = 0;

  // Revokes all SSL certificate error allow exceptions made by the user for
  // |host|.
  virtual void RevokeUserAllowExceptions(const std::string& host) = 0;

  // Sets HTTPS-First Mode enforcement for the given `host`.
  virtual void SetHttpsEnforcementForHost(
      const std::string& host,
      bool enforce,
      StoragePartition* storage_partition) = 0;
  // Returns whether HTTPS-First Mode is enabled for the given `url`. This check
  // ignores the scheme of `url`. E.g. http://example.com and
  // https://example.com will return the same result.
  virtual bool IsHttpsEnforcedForUrl(const GURL& url,
                                     StoragePartition* storage_partition) = 0;

  // Returns whether the user has allowed a certificate error exception or
  // HTTP exception for |host|. This does not mean that *all* certificate errors
  // are allowed, just that there exists an exception. To see if a particular
  // certificate and error combination exception is allowed, use QueryPolicy().
  virtual bool HasAllowException(const std::string& host,
                                 StoragePartition* storage_partition) = 0;

  // Returns true if the user has allowed a certificate error exception or HTTP
  // exception for any host.
  virtual bool HasAllowExceptionForAnyHost(
      StoragePartition* storage_partition) = 0;

 protected:
  virtual ~SSLHostStateDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SSL_HOST_STATE_DELEGATE_H_
