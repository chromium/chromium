// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config.h"

#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/cert/cert_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CheckCertVerifyFlags(SSLConfig* ssl_config,
                          bool disable_cert_verification_network_fetches) {
  ssl_config->disable_cert_verification_network_fetches =
      disable_cert_verification_network_fetches;

  int flags = ssl_config->GetCertVerifyFlags();
  EXPECT_EQ(disable_cert_verification_network_fetches,
            !!(flags & CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES));
}

}  // namespace

TEST(SSLConfigTest, GetCertVerifyFlags) {
  SSLConfig ssl_config;
  CheckCertVerifyFlags(&ssl_config,
                       /*disable_cert_verification_network_fetches*/ false);
  CheckCertVerifyFlags(&ssl_config,
                       /*disable_cert_verification_network_fetches*/ true);
}

TEST(SSLConfigTest, DefaultProxyChain) {
  SSLConfig ssl_config;
  EXPECT_TRUE(ssl_config.proxy_chain.is_direct());
  EXPECT_EQ(ssl_config.proxy_chain_index, 0);
}

TEST(SSLConfigTest, DefaultSessionUsage) {
  SSLConfig ssl_config;
  EXPECT_EQ(ssl_config.session_usage, SessionUsage::kDestination);
}

}  // namespace net
