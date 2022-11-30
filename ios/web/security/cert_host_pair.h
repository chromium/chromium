// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SECURITY_CERT_HOST_PAIR_H_
#define IOS_WEB_SECURITY_CERT_HOST_PAIR_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "net/base/hash_value.h"

namespace net {
class X509Certificate;
}

namespace web {

// Holds certificate-host pair. Implements operator less, hence can act as a key
// for a container.
class CertHostPair {
 public:
  CertHostPair(scoped_refptr<net::X509Certificate> cert, std::string host);
  CertHostPair(const CertHostPair& other);
  ~CertHostPair();

  bool operator<(const CertHostPair& other) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CertHostPairTest, Construction);

  const scoped_refptr<net::X509Certificate> cert_;
  const std::string host_;
  const net::SHA256HashValue cert_hash_;
};

}  // namespace web

#endif  // IOS_WEB_SECURITY_CERT_HOST_PAIR_H_
