// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_CERTIFICATE_NET_LOG_PARAM_H_
#define NET_CERT_X509_CERTIFICATE_NET_LOG_PARAM_H_

#include "net/base/net_export.h"

namespace base {
class Value;
}

namespace net {

class X509Certificate;

// Creates a base::Value::Type::LIST NetLog parameter to describe an
// X509Certificate chain.
NET_EXPORT base::Value NetLogX509CertificateList(
    const X509Certificate* certificate);

}  // namespace net

#endif  // NET_CERT_X509_CERTIFICATE_NET_LOG_PARAM_H_
