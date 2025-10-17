// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/proxy_fallback.h"

#include <optional>

#include "net/base/mock_proxy_delegate.h"
#include "net/base/proxy_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using ::testing::Return;

TEST(ProxyFallbackTest,
     NoFallbackByDefault_ProxyDelegateWithoutOverride_NoFallback) {
  int error = ERR_TUNNEL_CONNECTION_FAILED;
  int result = error;
  EXPECT_FALSE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                      /*proxy_delegate=*/nullptr));

  MockProxyDelegate no_override_delegate;
  EXPECT_CALL(no_override_delegate,
              CanFalloverToNextProxyOverride(ProxyChain::Direct(),
                                             ERR_TUNNEL_CONNECTION_FAILED))
      .WillOnce(Return(std::nullopt));
  EXPECT_FALSE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                      &no_override_delegate));
}

TEST(ProxyFallbackTest,
     NoFallbackByDefault_ProxyDelegateOverrideToFallback_Fallback) {
  int error = ERR_TUNNEL_CONNECTION_FAILED;
  int result = error;
  EXPECT_FALSE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                      /*proxy_delegate=*/nullptr));

  MockProxyDelegate always_fallback_delegate;
  EXPECT_CALL(always_fallback_delegate,
              CanFalloverToNextProxyOverride(ProxyChain::Direct(),
                                             ERR_TUNNEL_CONNECTION_FAILED))
      .WillOnce(Return(true));
  EXPECT_TRUE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                     &always_fallback_delegate));
}

TEST(ProxyFallbackTest,
     FallbackByDefault_ProxyDelegateWithoutOverride_Fallback) {
  int error = ERR_PROXY_CONNECTION_FAILED;
  int result = error;
  EXPECT_TRUE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                     /*proxy_delegate=*/nullptr));

  MockProxyDelegate no_override_delegate;
  EXPECT_CALL(no_override_delegate,
              CanFalloverToNextProxyOverride(ProxyChain::Direct(),
                                             ERR_PROXY_CONNECTION_FAILED))
      .WillOnce(Return(std::nullopt));
  EXPECT_TRUE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                     &no_override_delegate));
}

TEST(ProxyFallbackTest,
     FallbackByDefault_ProxyDelegateOverrideToNoFallback_Fallback) {
  int error = ERR_PROXY_CONNECTION_FAILED;
  int result = error;
  EXPECT_TRUE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                     /*proxy_delegate=*/nullptr));

  MockProxyDelegate always_fallback_delegate;
  EXPECT_CALL(always_fallback_delegate,
              CanFalloverToNextProxyOverride(ProxyChain::Direct(),
                                             ERR_PROXY_CONNECTION_FAILED))
      .WillOnce(Return(false));
  EXPECT_FALSE(CanFalloverToNextProxy(ProxyChain::Direct(), error, &result,
                                      &always_fallback_delegate));
}

}  // namespace

}  // namespace net
