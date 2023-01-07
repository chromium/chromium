// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/alternative_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(AlternativeServicesTest, IsProtocolEnabledHttp11) {
  EXPECT_TRUE(IsProtocolEnabled(kProtoHTTP11, /*is_http2_enabled=*/false,
                                /*is_quic_enabled=*/false));
  EXPECT_TRUE(IsProtocolEnabled(kProtoHTTP11, /*is_http2_enabled=*/false,
                                /*is_quic_enabled=*/true));
  EXPECT_TRUE(IsProtocolEnabled(kProtoHTTP11, /*is_http2_enabled=*/true,
                                /*is_quic_enabled=*/false));
  EXPECT_TRUE(IsProtocolEnabled(kProtoHTTP11, /*is_http2_enabled=*/true,
                                /*is_quic_enabled=*/true));
}

TEST(AlternativeServicesTest, IsProtocolEnabledHttp2) {
  EXPECT_FALSE(IsProtocolEnabled(kProtoHTTP2, /*is_http2_enabled=*/false,
                                 /*is_quic_enabled=*/false));
  EXPECT_FALSE(IsProtocolEnabled(kProtoHTTP2, /*is_http2_enabled=*/false,
                                 /*is_quic_enabled=*/true));
  EXPECT_TRUE(IsProtocolEnabled(kProtoHTTP2, /*is_http2_enabled=*/true,
                                /*is_quic_enabled=*/false));
  EXPECT_TRUE(IsProtocolEnabled(kProtoHTTP2, /*is_http2_enabled=*/true,
                                /*is_quic_enabled=*/true));
}

TEST(AlternativeServicesTest, IsProtocolEnabledQuic) {
  EXPECT_FALSE(IsProtocolEnabled(kProtoQUIC, /*is_http2_enabled=*/false,
                                 /*is_quic_enabled=*/false));
  EXPECT_TRUE(IsProtocolEnabled(kProtoQUIC, /*is_http2_enabled=*/false,
                                /*is_quic_enabled=*/true));
  EXPECT_FALSE(IsProtocolEnabled(kProtoQUIC, /*is_http2_enabled=*/true,
                                 /*is_quic_enabled=*/false));
  EXPECT_TRUE(IsProtocolEnabled(kProtoQUIC, /*is_http2_enabled=*/true,
                                /*is_quic_enabled=*/true));
}

}  // namespace
}  // namespace net
