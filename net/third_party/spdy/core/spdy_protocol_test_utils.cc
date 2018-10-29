// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/spdy_protocol_test_utils.h"

#include <cstdint>

#include "net/third_party/spdy/platform/api/spdy_string_piece.h"

namespace spdy {
namespace test {

// TODO(jamessynge): Where it makes sense in these functions, it would be nice
// to make use of the existing gMock matchers here, instead of rolling our own.

::testing::AssertionResult VerifySpdyFrameWithHeaderBlockIREquals(
    const SpdyFrameWithHeaderBlockIR& expected,
    const SpdyFrameWithHeaderBlockIR& actual) {
  VLOG(1) << "VerifySpdyFrameWithHeaderBlockIREquals";
  VERIFY_TRUE(actual.header_block() == expected.header_block());
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyAltSvcIR& expected,
                                                   const SpdyAltSvcIR& actual) {
  VERIFY_EQ(expected.stream_id(), actual.stream_id());
  VERIFY_EQ(expected.origin(), actual.origin());
  VERIFY_THAT(actual.altsvc_vector(),
              ::testing::ContainerEq(expected.altsvc_vector()));
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyContinuationIR& expected,
    const SpdyContinuationIR& actual) {
  return ::testing::AssertionFailure()
         << "VerifySpdyFrameIREquals SpdyContinuationIR not yet implemented";
}

::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyDataIR& expected,
                                                   const SpdyDataIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyDataIR";
  VERIFY_EQ(expected.stream_id(), actual.stream_id());
  VERIFY_EQ(expected.fin(), actual.fin());
  VERIFY_EQ(expected.data_len(), actual.data_len());
  if (expected.data() == nullptr) {
    VERIFY_EQ(nullptr, actual.data());
  } else {
    VERIFY_EQ(SpdyStringPiece(expected.data(), expected.data_len()),
              SpdyStringPiece(actual.data(), actual.data_len()));
  }
  VERIFY_SUCCESS(VerifySpdyFrameWithPaddingIREquals(expected, actual));
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyGoAwayIR& expected,
                                                   const SpdyGoAwayIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyGoAwayIR";
  VERIFY_EQ(expected.last_good_stream_id(), actual.last_good_stream_id());
  VERIFY_EQ(expected.error_code(), actual.error_code());
  VERIFY_EQ(expected.description(), actual.description());
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyHeadersIR& expected,
    const SpdyHeadersIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyHeadersIR";
  VERIFY_EQ(expected.stream_id(), actual.stream_id());
  VERIFY_EQ(expected.fin(), actual.fin());
  VERIFY_SUCCESS(VerifySpdyFrameWithHeaderBlockIREquals(expected, actual));
  VERIFY_EQ(expected.has_priority(), actual.has_priority());
  if (expected.has_priority()) {
    VERIFY_SUCCESS(VerifySpdyFrameWithPriorityIREquals(expected, actual));
  }
  VERIFY_SUCCESS(VerifySpdyFrameWithPaddingIREquals(expected, actual));
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyPingIR& expected,
                                                   const SpdyPingIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyPingIR";
  VERIFY_EQ(expected.id(), actual.id());
  VERIFY_EQ(expected.is_ack(), actual.is_ack());
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyPriorityIR& expected,
    const SpdyPriorityIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyPriorityIR";
  VERIFY_EQ(expected.stream_id(), actual.stream_id());
  VERIFY_SUCCESS(VerifySpdyFrameWithPriorityIREquals(expected, actual));
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyPushPromiseIR& expected,
    const SpdyPushPromiseIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyPushPromiseIR";
  VERIFY_EQ(expected.stream_id(), actual.stream_id());
  VERIFY_SUCCESS(VerifySpdyFrameWithPaddingIREquals(expected, actual));
  VERIFY_EQ(expected.promised_stream_id(), actual.promised_stream_id());
  VERIFY_SUCCESS(VerifySpdyFrameWithHeaderBlockIREquals(expected, actual));
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyRstStreamIR& expected,
    const SpdyRstStreamIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyRstStreamIR";
  VERIFY_EQ(expected.stream_id(), actual.stream_id());
  VERIFY_EQ(expected.error_code(), actual.error_code());
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdySettingsIR& expected,
    const SpdySettingsIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdySettingsIR";
  // Note, ignoring non-HTTP/2 fields such as clear_settings.
  VERIFY_EQ(expected.is_ack(), actual.is_ack());

  // Note, the following doesn't work because there isn't a comparator and
  // formatter for SpdySettingsIR::Value. Fixable if we cared.
  //
  //   VERIFY_THAT(actual.values(), ::testing::ContainerEq(actual.values()));

  VERIFY_EQ(expected.values().size(), actual.values().size());
  for (const auto& entry : expected.values()) {
    const auto& param = entry.first;
    auto actual_itr = actual.values().find(param);
    VERIFY_TRUE(!(actual_itr == actual.values().end()))
        << "actual doesn't contain param: " << param;
    uint32_t expected_value = entry.second;
    uint32_t actual_value = actual_itr->second;
    VERIFY_EQ(expected_value, actual_value)
        << "Values don't match for parameter: " << param;
  }

  return ::testing::AssertionSuccess();
}

::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyWindowUpdateIR& expected,
    const SpdyWindowUpdateIR& actual) {
  VLOG(1) << "VerifySpdyFrameIREquals SpdyWindowUpdateIR";
  VERIFY_EQ(expected.stream_id(), actual.stream_id());
  VERIFY_EQ(expected.delta(), actual.delta());
  return ::testing::AssertionSuccess();
}

}  // namespace test
}  // namespace spdy
