// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/session/session_certificate_policy_cache_impl.h"

#include "base/functional/bind.h"
#include "ios/web/public/security/certificate_policy_cache.h"
#include "ios/web/public/session/proto/session.pb.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/session/session_certificate.h"

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

SessionCertificatePolicyCacheImpl::SessionCertificatePolicyCacheImpl(
    BrowserState* browser_state,
    const proto::CertificatesCacheStorage& storage)
    : SessionCertificatePolicyCacheImpl(browser_state) {
  for (const proto::CertificateStorage& cert_storage : storage.certs()) {
    SessionCertificate certificate(cert_storage);
    if (certificate.certificate() && !certificate.host().empty()) {
      allowed_certs_.insert(SessionCertificate(cert_storage));
    }
  }
}

void SessionCertificatePolicyCacheImpl::SerializeToProto(
    proto::CertificatesCacheStorage& storage) const {
  for (const SessionCertificate& cert : allowed_certs_) {
    cert.SerializeToProto(*storage.add_certs());
  }
}

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

}  // namespace web
