// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_IOS_H_
#define NET_CERT_X509_UTIL_IOS_H_

#include <Security/Security.h>

#include <vector>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace net {

class X509Certificate;

namespace x509_util {

// Creates an X509Certificate representing |sec_cert| with intermediates
// |sec_chain|.
NET_EXPORT scoped_refptr<X509Certificate>
CreateX509CertificateFromSecCertificate(
    base::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::ScopedCFTypeRef<SecCertificateRef>>& sec_chain);

}  // namespace x509_util

}  // namespace net

#endif  // NET_CERT_X509_UTIL_IOS_H_
