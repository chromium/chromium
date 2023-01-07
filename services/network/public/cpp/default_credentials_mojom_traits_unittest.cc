// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/default_credentials_mojom_traits.h"

#include <vector>

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/default_credentials_mojom_traits.h"
#include "services/network/public/mojom/default_credentials.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

template <typename MojoType, typename NativeType>
bool SerializeAndDeserializeEnum(NativeType in, NativeType* out) {
  MojoType intermediate = mojo::EnumTraits<MojoType, NativeType>::ToMojom(in);
  return mojo::EnumTraits<MojoType, NativeType>::FromMojom(intermediate, out);
}

TEST(DefaultCredentialsTraitsTest, Roundtrips_DefaultCredentials) {
  for (net::HttpAuthPreferences::DefaultCredentials default_credentials :
       {net::HttpAuthPreferences::ALLOW_DEFAULT_CREDENTIALS,
        net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS}) {
    net::HttpAuthPreferences::DefaultCredentials roundtrip;
    ASSERT_TRUE(SerializeAndDeserializeEnum<mojom::DefaultCredentials>(
        default_credentials, &roundtrip));
    EXPECT_EQ(default_credentials, roundtrip);
  }
}

}  // namespace
}  // namespace network
