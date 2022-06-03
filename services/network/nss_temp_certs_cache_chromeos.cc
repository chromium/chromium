// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/nss_temp_certs_cache_chromeos.h"

#include "base/logging.h"
#include "net/cert/x509_util_nss.h"

namespace network {

NSSTempCertsCacheChromeOS::NSSTempCertsCacheChromeOS(
    const net::CertificateList& certificates) {
  for (const auto& certificate : certificates) {
    net::ScopedCERTCertificate x509_cert =
        net::x509_util::CreateCERTCertificateFromX509Certificate(
            certificate.get());
    if (!x509_cert) {
      LOG(ERROR) << "Unable to create CERTCertificate";
      continue;
    }

    temp_certs_.push_back(std::move(x509_cert));
  }
}

NSSTempCertsCacheChromeOS::~NSSTempCertsCacheChromeOS() {}

}  // namespace network
