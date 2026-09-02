// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/load_timing_internal_info.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "services/network/public/mojom/load_timing_internal_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(LoadTimingInternalInfoMojomTraitsTest,
     MultiplexedSessionCreationInitiatorRoundTrip) {
  for (uint32_t i = 0;
       i <= static_cast<uint32_t>(
                net::MultiplexedSessionCreationInitiator::kMaxValue);
       ++i) {
    auto original = static_cast<net::MultiplexedSessionCreationInitiator>(i);
    net::MultiplexedSessionCreationInitiator deserialized;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::MultiplexedSessionCreationInitiator>(original,
                                                            deserialized));
    EXPECT_EQ(deserialized, original);
  }
}

TEST(LoadTimingInternalInfoMojomTraitsTest,
     QuicSessionEstablishmentReasonRoundTrip) {
  for (uint32_t i = 0; i <= static_cast<uint32_t>(
                                net::QuicSessionEstablishmentReason::kMaxValue);
       ++i) {
    auto original = static_cast<net::QuicSessionEstablishmentReason>(i);
    net::QuicSessionEstablishmentReason deserialized;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                mojom::QuicSessionEstablishmentReason>(original, deserialized));
    EXPECT_EQ(deserialized, original);
  }
}

TEST(LoadTimingInternalInfoMojomTraitsTest,
     QuicSessionNonReuseReasonRoundTrip) {
  for (uint32_t i = 0;
       i <= static_cast<uint32_t>(net::QuicSessionNonReuseReason::kMaxValue);
       ++i) {
    auto original = static_cast<net::QuicSessionNonReuseReason>(i);
    net::QuicSessionNonReuseReason deserialized;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::QuicSessionNonReuseReason>(
            original, deserialized));
    EXPECT_EQ(deserialized, original);
  }
}

TEST(LoadTimingInternalInfoMojomTraitsTest,
     QuicConnectionReuseDetailsNulloptAndValues) {
  // Nullopt fields.
  net::QuicConnectionReuseDetails original_empty;
  net::QuicConnectionReuseDetails deserialized_empty;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::QuicConnectionReuseDetails>(
          original_empty, deserialized_empty));
  EXPECT_EQ(deserialized_empty, original_empty);

  // Populated fields.
  net::QuicConnectionReuseDetails original_populated;
  original_populated.establishment_reason =
      net::QuicSessionEstablishmentReason::kSessionExistedButNotPreconnect;
  original_populated.non_reuse_reason =
      net::QuicSessionNonReuseReason::kSessionExisted_MultipleReasons;
  net::QuicConnectionReuseDetails deserialized_populated;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::QuicConnectionReuseDetails>(
          original_populated, deserialized_populated));
  EXPECT_EQ(deserialized_populated, original_populated);
}

TEST(LoadTimingInternalInfoMojomTraitsTest, DefaultLoadTimingInternalInfo) {
  net::LoadTimingInternalInfo default_info;
  net::LoadTimingInternalInfo deserialized;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          default_info, deserialized));
  EXPECT_EQ(deserialized, default_info);
  EXPECT_FALSE(deserialized.quic_connection_reuse_details.has_value());
  EXPECT_FALSE(deserialized.session_creation_initiator.has_value());
}

TEST(LoadTimingInternalInfoMojomTraitsTest, SerializeAndDeserialize) {
  net::LoadTimingInternalInfo original;
  original.max_stream_limit_pending_delay = base::Seconds(1);
  original.create_stream_delay = base::Seconds(1);
  original.connected_callback_delay = base::Seconds(1);
  original.initialize_stream_delay = base::Seconds(1);
  original.session_source = net::SessionSource::kNew;
  original.advertised_alt_svc_state =
      net::AdvertisedAltSvcState::kQuicNotBroken;
  original.http_network_session_quic_enabled = true;
  original.resolution_details = net::ResolutionDetails{
      .source = net::ResolutionSource::kSecure,
      .task_completion_delay = base::Milliseconds(123),
      .secure_dns_attempted = true,
      .doh_details = net::DohResolutionDetails{
          .session_source = net::SessionSource::kNew,
          .connection_info = net::HttpConnectionInfoCoarse::kQUIC,
      }};
  net::QuicConnectionReuseDetails quic_details;
  quic_details.establishment_reason =
      net::QuicSessionEstablishmentReason::kSessionExistedButNotPreconnect;
  quic_details.non_reuse_reason =
      net::QuicSessionNonReuseReason::kNoSessionExisted_KeyMismatch_SocketTag;
  original.quic_connection_reuse_details = quic_details;
  original.session_creation_initiator =
      net::MultiplexedSessionCreationInitiator::kPreconnect;

  net::LoadTimingInternalInfo deserialized;
  ASSERT_NE(deserialized, original);
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          original, deserialized));
  EXPECT_EQ(deserialized, original);

  // Test serialization of kNoSessionExisted_KeyMismatch_MultipleFields.
  original.quic_connection_reuse_details->non_reuse_reason = net::
      QuicSessionNonReuseReason::kNoSessionExisted_KeyMismatch_MultipleFields;
  net::LoadTimingInternalInfo deserialized_multi;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          original, deserialized_multi));
  EXPECT_EQ(deserialized_multi, original);

  // Test serialization of kSessionExisted_MultipleReasons.
  original.quic_connection_reuse_details->non_reuse_reason =
      net::QuicSessionNonReuseReason::kSessionExisted_MultipleReasons;
  net::LoadTimingInternalInfo deserialized_multi_reasons;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          original, deserialized_multi_reasons));
  EXPECT_EQ(deserialized_multi_reasons, original);

  // Test serialization of in-flight establishment reasons.
  original.quic_connection_reuse_details->establishment_reason =
      net::QuicSessionEstablishmentReason::kInflightSessionAndWasPreconnect;
  net::LoadTimingInternalInfo deserialized_inflight_preconnect;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          original, deserialized_inflight_preconnect));
  EXPECT_EQ(deserialized_inflight_preconnect, original);

  original.quic_connection_reuse_details->establishment_reason =
      net::QuicSessionEstablishmentReason::kInflightSessionButNotPreconnect;
  net::LoadTimingInternalInfo deserialized_inflight_non_preconnect;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          original, deserialized_inflight_non_preconnect));
  EXPECT_EQ(deserialized_inflight_non_preconnect, original);
}

}  // namespace
}  // namespace network
