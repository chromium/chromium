// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate_net_log_param.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_capture_mode.h"

namespace net {

base::Value NetLogX509CertificateList(const X509Certificate* certificate) {
  base::Value::List certs;
  std::vector<std::string> encoded_chain;
  certificate->GetPEMEncodedChain(&encoded_chain);
  for (auto& pem : encoded_chain)
    certs.Append(std::move(pem));
  return base::Value(std::move(certs));
}

}  // namespace net
