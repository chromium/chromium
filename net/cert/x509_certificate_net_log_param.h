// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_CERTIFICATE_NET_LOG_PARAM_H_
#define NET_CERT_X509_CERTIFICATE_NET_LOG_PARAM_H_

#include <memory>

#include "net/base/net_export.h"

namespace base {
class Value;
}

namespace net {

class X509Certificate;

// Creates NetLog parameter to describe an X509Certificate.
NET_EXPORT base::Value NetLogX509CertificateParams(
    const X509Certificate* certificate);

}  // namespace net

#endif  // NET_CERT_X509_CERTIFICATE_NET_LOG_PARAM_H_
