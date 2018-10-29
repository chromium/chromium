// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_SPDY_PROTOCOL_TEST_UTILS_H_
#define NET_THIRD_PARTY_SPDY_CORE_SPDY_PROTOCOL_TEST_UTILS_H_

// These functions support tests that need to compare two concrete SpdyFrameIR
// instances for equality. They return AssertionResult, so they may be used as
// follows:
//
//    SomeSpdyFrameIRSubClass expected_ir(...);
//    std::unique_ptr<SpdyFrameIR> collected_frame;
//    ... some test code that may fill in collected_frame ...
//    ASSERT_TRUE(VerifySpdyFrameIREquals(expected_ir, collected_frame.get()));
//
// TODO(jamessynge): Where it makes sense in these functions, it would be nice
// to make use of the existing gMock matchers here, instead of rolling our own.

#include <typeinfo>

#include "base/logging.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace spdy {
namespace test {

// Verify the header entries in two SpdyFrameWithHeaderBlockIR instances
// are the same.
::testing::AssertionResult VerifySpdyFrameWithHeaderBlockIREquals(
    const SpdyFrameWithHeaderBlockIR& expected,
    const SpdyFrameWithHeaderBlockIR& actual);

// Verify that the padding in two frames of type T is the same.
template <class T>
::testing::AssertionResult VerifySpdyFrameWithPaddingIREquals(const T& expected,
                                                              const T& actual) {
  VLOG(1) << "VerifySpdyFrameWithPaddingIREquals";
  VERIFY_EQ(expected.padded(), actual.padded());
  if (expected.padded()) {
    VERIFY_EQ(expected.padding_payload_len(), actual.padding_payload_len());
  }

  return ::testing::AssertionSuccess();
}

// Verify the priority fields in two frames of type T are the same.
template <class T>
::testing::AssertionResult VerifySpdyFrameWithPriorityIREquals(
    const T& expected,
    const T& actual) {
  VLOG(1) << "VerifySpdyFrameWithPriorityIREquals";
  VERIFY_EQ(expected.parent_stream_id(), actual.parent_stream_id());
  VERIFY_EQ(expected.weight(), actual.weight());
  VERIFY_EQ(expected.exclusive(), actual.exclusive());
  return ::testing::AssertionSuccess();
}

// Verify that two SpdyAltSvcIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyAltSvcIR& expected,
                                                   const SpdyAltSvcIR& actual);

// VerifySpdyFrameIREquals for SpdyContinuationIR frames isn't really needed
// because we don't really make use of SpdyContinuationIR, instead creating
// SpdyHeadersIR or SpdyPushPromiseIR with the pre-encoding form of the HPACK
// block (i.e. we don't yet have a CONTINUATION frame).
//
// ::testing::AssertionResult VerifySpdyFrameIREquals(
//     const SpdyContinuationIR& expected,
//     const SpdyContinuationIR& actual) {
//   return ::testing::AssertionFailure()
//          << "VerifySpdyFrameIREquals SpdyContinuationIR NYI";
// }

// Verify that two SpdyDataIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyDataIR& expected,
                                                   const SpdyDataIR& actual);

// Verify that two SpdyGoAwayIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyGoAwayIR& expected,
                                                   const SpdyGoAwayIR& actual);

// Verify that two SpdyHeadersIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyHeadersIR& expected,
    const SpdyHeadersIR& actual);

// Verify that two SpdyPingIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(const SpdyPingIR& expected,
                                                   const SpdyPingIR& actual);

// Verify that two SpdyPriorityIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyPriorityIR& expected,
    const SpdyPriorityIR& actual);

// Verify that two SpdyPushPromiseIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyPushPromiseIR& expected,
    const SpdyPushPromiseIR& actual);

// Verify that two SpdyRstStreamIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyRstStreamIR& expected,
    const SpdyRstStreamIR& actual);

// Verify that two SpdySettingsIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdySettingsIR& expected,
    const SpdySettingsIR& actual);

// Verify that two SpdyWindowUpdateIR frames are the same.
::testing::AssertionResult VerifySpdyFrameIREquals(
    const SpdyWindowUpdateIR& expected,
    const SpdyWindowUpdateIR& actual);

// Verify that either expected and actual are both nullptr, or that both are not
// nullptr, and that actual is of type E, and that it matches expected.
template <class E>
::testing::AssertionResult VerifySpdyFrameIREquals(const E* expected,
                                                   const SpdyFrameIR* actual) {
  if (expected == nullptr || actual == nullptr) {
    VLOG(1) << "VerifySpdyFrameIREquals one null";
    VERIFY_EQ(expected, nullptr);
    VERIFY_EQ(actual, nullptr);
    return ::testing::AssertionSuccess();
  }
  VLOG(1) << "VerifySpdyFrameIREquals not null";
  VERIFY_EQ(actual->frame_type(), expected->frame_type());
  const E* actual2 = static_cast<const E*>(actual);
  return VerifySpdyFrameIREquals(*expected, *actual2);
}

// Verify that actual is not nullptr, that it is of type E and that it
// matches expected.
template <class E>
::testing::AssertionResult VerifySpdyFrameIREquals(const E& expected,
                                                   const SpdyFrameIR* actual) {
  VLOG(1) << "VerifySpdyFrameIREquals";
  return VerifySpdyFrameIREquals(&expected, actual);
}

}  // namespace test
}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_SPDY_PROTOCOL_TEST_UTILS_H_
