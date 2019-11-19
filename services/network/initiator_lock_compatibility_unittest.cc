// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/initiator_lock_compatibility.h"

#include "base/optional.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

TEST(InitiatorLockCompatibilityTest, VerifyRequestInitiatorSiteLock) {
  url::Origin opaque_origin = url::Origin();
  url::Origin opaque_origin2 = url::Origin();

  url::Origin ip_origin1 = url::Origin::Create(GURL("http://127.0.0.1/"));
  url::Origin ip_origin2 = url::Origin::Create(GURL("http://217.17.45.162/"));

  url::Origin example_com = url::Origin::Create(GURL("http://example.com"));
  url::Origin foo_example_com =
      url::Origin::Create(GURL("http://foo.example.com"));
  url::Origin bar_example_com =
      url::Origin::Create(GURL("http://bar.example.com"));
  url::Origin foo_example_com_dot =
      url::Origin::Create(GURL("http://foo.example.com."));
  url::Origin bar_foo_example_com =
      url::Origin::Create(GURL("http://bar.foo.example.com"));

  url::Origin other_site = url::Origin::Create(GURL("http://other.com"));

  // Cases without a lock.
  EXPECT_EQ(InitiatorLockCompatibility::kNoLock,
            VerifyRequestInitiatorLock(base::nullopt, base::nullopt));

  // Opaque initiator is always safe (and so results in kCompatibleLock).
  // OTOH, opaque lock is only compatible with an opaque initiator.
  EXPECT_EQ(InitiatorLockCompatibility::kCompatibleLock,
            VerifyRequestInitiatorLock(bar_foo_example_com, opaque_origin));
  EXPECT_EQ(InitiatorLockCompatibility::kCompatibleLock,
            VerifyRequestInitiatorLock(opaque_origin, opaque_origin2));
  EXPECT_EQ(InitiatorLockCompatibility::kIncorrectLock,
            VerifyRequestInitiatorLock(opaque_origin, bar_foo_example_com));

  // Regular origin equality.
  EXPECT_EQ(
      InitiatorLockCompatibility::kCompatibleLock,
      VerifyRequestInitiatorLock(bar_foo_example_com, bar_foo_example_com));

  // Regular origin inequality.
  EXPECT_EQ(InitiatorLockCompatibility::kIncorrectLock,
            VerifyRequestInitiatorLock(bar_foo_example_com, other_site));

  // IP addresses have to be special-cased in some places (e.g. they shouldn't
  // be subject to DomainIs / eTLD+1 comparisons).
  EXPECT_EQ(InitiatorLockCompatibility::kIncorrectLock,
            VerifyRequestInitiatorLock(ip_origin1, ip_origin2));
  EXPECT_EQ(InitiatorLockCompatibility::kCompatibleLock,
            VerifyRequestInitiatorLock(ip_origin1, ip_origin1));

  // Compatibility check shouldn't strip the lock down to eTLD+1.
  EXPECT_EQ(InitiatorLockCompatibility::kIncorrectLock,
            VerifyRequestInitiatorLock(foo_example_com, bar_example_com));

  // Site-URL-based comparisons.
  //
  // TODO(lukasza): These should result in kIncorrectLock eventually (once
  // request_initiator_site_lock becomes request_initiator_origin_lock - see
  // https://crbug.com/888079 and https://crbug.com/891872.
  EXPECT_EQ(InitiatorLockCompatibility::kCompatibleLock,
            VerifyRequestInitiatorLock(example_com, bar_foo_example_com));
  EXPECT_EQ(InitiatorLockCompatibility::kCompatibleLock,
            VerifyRequestInitiatorLock(foo_example_com, bar_foo_example_com));

  // The trailing dot is not important (at least for site-URL-based
  // comparisons).
  EXPECT_EQ(InitiatorLockCompatibility::kCompatibleLock,
            VerifyRequestInitiatorLock(foo_example_com_dot, foo_example_com));
  EXPECT_EQ(InitiatorLockCompatibility::kCompatibleLock,
            VerifyRequestInitiatorLock(foo_example_com, foo_example_com_dot));
}

}  // namespace network
