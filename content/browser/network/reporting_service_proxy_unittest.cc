// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ReportingServiceProxyTest, ValidNetworkIsolationKeyProducesValidNAK) {
  net::NetworkIsolationKey valid_nik =
      net::NetworkIsolationKey::CreateTransientForTesting();
  EXPECT_FALSE(valid_nik.IsEmpty());

  net::NetworkAnonymizationKey nak =
      net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(valid_nik);
  EXPECT_FALSE(nak.IsEmpty());
}

}  // namespace content
