// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/load_timing_internal_info.h"
#include "services/network/public/mojom/load_timing_internal_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(LoadTimingInternalInfoMojomTraitsTest, SerializeAndDeserialize) {
  net::LoadTimingInternalInfo original;
  original.create_stream_delay = base::Seconds(1);
  original.connected_callback_delay = base::Seconds(1);
  original.initialize_stream_delay = base::Seconds(1);
  original.session_source = net::SessionSource::kNew;
  original.advertised_alt_svc_state =
      net::AdvertisedAltSvcState::kQuicNotBroken;
  original.http_network_session_quic_enabled = true;

  net::LoadTimingInternalInfo deserialized;
  ASSERT_NE(deserialized, original);
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          original, deserialized));
  EXPECT_EQ(deserialized, original);
}

}  // namespace
}  // namespace network
