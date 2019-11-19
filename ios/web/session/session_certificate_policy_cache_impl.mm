// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/session/session_certificate_policy_cache_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Break if CertStatus values changed, as they are persisted on disk and thus
// must be consistent.
static_assert(net::CERT_STATUS_ALL_ERRORS == 0xFF00FFFF,
              "The value of CERT_STATUS_ALL_ERRORS changed!");
static_assert(net::CERT_STATUS_COMMON_NAME_INVALID == 1 << 0,
              "The value of CERT_STATUS_COMMON_NAME_INVALID changed!");
static_assert(net::CERT_STATUS_DATE_INVALID == 1 << 1,
              "The value of CERT_STATUS_DATE_INVALID changed!");
static_assert(net::CERT_STATUS_AUTHORITY_INVALID == 1 << 2,
              "The value of CERT_STATUS_AUTHORITY_INVALID changed!");
static_assert(net::CERT_STATUS_NO_REVOCATION_MECHANISM == 1 << 4,
              "The value of CERT_STATUS_NO_REVOCATION_MECHANISM changed!");
static_assert(net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION == 1 << 5,
              "The value of CERT_STATUS_UNABLE_TO_CHECK_REVOCATION changed!");
static_assert(net::CERT_STATUS_REVOKED == 1 << 6,
              "The value of CERT_STATUS_REVOKED changed!");
static_assert(net::CERT_STATUS_INVALID == 1 << 7,
              "The value of CERT_STATUS_INVALID changed!");
static_assert(net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM == 1 << 8,
              "The value of CERT_STATUS_WEAK_SIGNATURE_ALGORITHM changed!");
static_assert(net::CERT_STATUS_NON_UNIQUE_NAME == 1 << 10,
              "The value of CERT_STATUS_NON_UNIQUE_NAME changed!");
static_assert(net::CERT_STATUS_WEAK_KEY == 1 << 11,
              "The value of CERT_STATUS_WEAK_KEY changed!");
static_assert(net::CERT_STATUS_IS_EV == 1 << 16,
              "The value of CERT_STATUS_IS_EV changed!");
static_assert(net::CERT_STATUS_REV_CHECKING_ENABLED == 1 << 17,
              "The value of CERT_STATUS_REV_CHECKING_ENABLED changed!");

namespace web {

SessionCertificatePolicyCacheImpl::SessionCertificatePolicyCacheImpl()
    : allowed_certs_([[NSMutableSet alloc] init]) {}

SessionCertificatePolicyCacheImpl::~SessionCertificatePolicyCacheImpl() {}

void SessionCertificatePolicyCacheImpl::UpdateCertificatePolicyCache(
    const scoped_refptr<web::CertificatePolicyCache>& cache) const {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(cache.get());
  NSSet* allowed_certs = [NSSet setWithSet:allowed_certs_];
  const scoped_refptr<web::CertificatePolicyCache> cache_copy = cache;
  base::PostTask(FROM_HERE, {web::WebThread::IO}, base::BindOnce(^{
                   for (CRWSessionCertificateStorage* cert in allowed_certs) {
                     cache_copy->AllowCertForHost(cert.certificate, cert.host,
                                                  cert.status);
                   }
                 }));
}

void SessionCertificatePolicyCacheImpl::RegisterAllowedCertificate(
    const scoped_refptr<net::X509Certificate> certificate,
    const std::string& host,
    net::CertStatus status) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  [allowed_certs_ addObject:[[CRWSessionCertificateStorage alloc]
                                initWithCertificate:certificate
                                               host:host
                                             status:status]];
}

void SessionCertificatePolicyCacheImpl::SetAllowedCerts(NSSet* allowed_certs) {
  allowed_certs_ = [allowed_certs mutableCopy];
}

NSSet* SessionCertificatePolicyCacheImpl::GetAllowedCerts() const {
  return allowed_certs_;
}

}  // namespace web
