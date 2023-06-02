// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/session/session_certificate_policy_cache_impl.h"

#import "base/functional/bind.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/session/session_certificate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// Registers `certificate` with `cache`.
void RegisterCertificate(scoped_refptr<CertificatePolicyCache> cache,
                         SessionCertificate certificate) {
  cache->AllowCertForHost(certificate.certificate().get(), certificate.host(),
                          certificate.status());
}

// Registers `certificates` with `cache`.
void RegisterCertificates(scoped_refptr<CertificatePolicyCache> cache,
                          SessionCertificateSet certificates) {
  for (const SessionCertificate& certificate : certificates) {
    cache->AllowCertForHost(certificate.certificate().get(), certificate.host(),
                            certificate.status());
  }
}

}  // anonymous namespace

SessionCertificatePolicyCacheImpl::SessionCertificatePolicyCacheImpl(
    BrowserState* browser_state)
    : SessionCertificatePolicyCache(browser_state) {}

SessionCertificatePolicyCacheImpl::~SessionCertificatePolicyCacheImpl() {}

void SessionCertificatePolicyCacheImpl::UpdateCertificatePolicyCache() const {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RegisterCertificates,
                                GetCertificatePolicyCache(), allowed_certs_));
}

void SessionCertificatePolicyCacheImpl::RegisterAllowedCertificate(
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::string& host,
    net::CertStatus status) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  SessionCertificate allowed_cert(certificate, host, status);
  allowed_certs_.insert(allowed_cert);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RegisterCertificate,
                                GetCertificatePolicyCache(), allowed_cert));
}

void SessionCertificatePolicyCacheImpl::SetAllowedCerts(
    NSSet<CRWSessionCertificateStorage*>* allowed_certs) {
  DCHECK(allowed_certs_.empty());
  for (CRWSessionCertificateStorage* cert_storage in allowed_certs) {
    allowed_certs_.insert(SessionCertificate(
        cert_storage.certificate, cert_storage.host, cert_storage.status));
  }
}

NSSet<CRWSessionCertificateStorage*>*
SessionCertificatePolicyCacheImpl::GetAllowedCerts() const {
  NSMutableSet<CRWSessionCertificateStorage*>* allowed_certs =
      [[NSMutableSet alloc] initWithCapacity:allowed_certs_.size()];

  for (const SessionCertificate& cert : allowed_certs_) {
    [allowed_certs addObject:[[CRWSessionCertificateStorage alloc]
                                 initWithCertificate:cert.certificate()
                                                host:cert.host()
                                              status:cert.status()]];
  }

  return allowed_certs;
}

}  // namespace web
