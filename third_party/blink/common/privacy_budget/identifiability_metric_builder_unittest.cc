// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"

#include <cinttypes>
#include <limits>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/privacy_budget/test_ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/scoped_identifiability_test_sample_collector.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace blink {

TEST(IdentifiabilityMetricBuilderTest, Set) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;

  IdentifiabilityMetricBuilder builder(ukm::SourceIdObj{});
  constexpr int64_t kInputHash = 2;
  constexpr int64_t kValue = 3;

  const auto kSurface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kInputHash);

  builder.Add(kSurface, kValue);
  builder.Record(&recorder);

  ASSERT_EQ(1u, collector.entries().size());
  auto& entry = collector.entries().front();

  EXPECT_EQ(entry.metrics.size(), 1u);
  EXPECT_EQ(entry.metrics.begin()->surface, kSurface);
  EXPECT_EQ(entry.metrics.begin()->value, kValue);
}

TEST(IdentifiabilityMetricBuilderTest, BuilderOverload) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;

  constexpr int64_t kValue = 3;
  constexpr int64_t kInputHash = 2;
  constexpr auto kSurface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kInputHash);

  const auto kSource = ukm::SourceIdObj::New();
  IdentifiabilityMetricBuilder(kSource).Add(kSurface, kValue).Record(&recorder);

  ASSERT_EQ(1u, collector.entries().size());
  test::ScopedIdentifiabilityTestSampleCollector::Entry expected_entry =
      collector.entries().front();
  collector.ClearEntries();

  // Yes, it seems cyclical, but this tests that the overloaded constructors
  // for IdentifiabilityMetricBuilder are equivalent.
  const ukm::SourceId kUkmSource = kSource.ToInt64();
  IdentifiabilityMetricBuilder(kUkmSource)
      .Add(kSurface, kValue)
      .Record(&recorder);
  ASSERT_EQ(1u, collector.entries().size());
  test::ScopedIdentifiabilityTestSampleCollector::Entry entry =
      collector.entries().front();

  EXPECT_EQ(expected_entry.source, entry.source);
}

TEST(IdentifiabilityMetricBuilderTest, SetWebfeature) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;

  constexpr int64_t kValue = 3;
  constexpr int64_t kTestInput =
      static_cast<int64_t>(mojom::WebFeature::kEventSourceDocument);

  IdentifiabilityMetricBuilder builder(ukm::SourceIdObj{});
  builder.AddWebFeature(mojom::WebFeature::kEventSourceDocument, kValue)
      .Record(&recorder);
  ASSERT_EQ(1u, collector.entries().size());
  auto entry = collector.entries().front();
  collector.ClearEntries();

  // Only testing that using SetWebfeature(x,y) is equivalent to
  // .Set(IdentifiableSurface::FromTypeAndToken(kWebFeature, x), y);
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(IdentifiableSurface::FromTypeAndToken(
               IdentifiableSurface::Type::kWebFeature, kTestInput),
           kValue)
      .Record(&recorder);
  ASSERT_EQ(1u, collector.entries().size());
  auto expected_entry = collector.entries().front();

  ASSERT_EQ(entry.metrics.size(), 1u);
  EXPECT_EQ(entry.metrics, expected_entry.metrics);
}

namespace {

// clang flags this function as unused although it's used in the MATCHER_P()
// definition below. Hence the [[maybe_unused]].
[[maybe_unused]] bool HasSingleEntryWithValue(
    const test::ScopedIdentifiabilityTestSampleCollector& collector,
    int64_t value) {
  if (collector.entries().size() != 1u) {
    SCOPED_TRACE(base::StringPrintf("Expected unique entry. Found %zu entries.",
                                    collector.entries().size()));
    return false;
  }
  if (collector.entries().front().metrics.size() != 1u) {
    SCOPED_TRACE(
        base::StringPrintf("Expected unique metric. Found %zu entries.",
                           collector.entries().front().metrics.size()));
    return false;
  }
  return collector.entries().front().metrics.front().value.ToUkmMetricValue() ==
         value;
}

MATCHER_P(FirstMetricIs,
          entry,
          base::StringPrintf("entry is %s0x%" PRIx64,
                             negation ? "not " : "",
                             entry)) {
  return HasSingleEntryWithValue(arg, entry);
}  // namespace

enum class Never { kGonna, kGive, kYou, kUp };

constexpr IdentifiableSurface kTestSurface =
    IdentifiableSurface::FromTypeAndToken(
        IdentifiableSurface::Type::kReservedInternal,
        0);

// Sample values
const char kAbcd[] = "abcd";
const int64_t kExpectedHashOfAbcd = -0x08a5c475eb66bd73;

// 5.1f
const int64_t kExpectedHashOfOnePointFive = 0x3ff8000000000000;

}  // namespace

TEST(IdentifiabilityMetricBuilderTest, SetChar) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, 'A')
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(65)));
}

TEST(IdentifiabilityMetricBuilderTest, SetCharArray) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiableToken sample(kAbcd);
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, sample)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(kExpectedHashOfAbcd));
}

TEST(IdentifiabilityMetricBuilderTest, SetStringPiece) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  // StringPiece() needs an explicit constructor invocation.
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, IdentifiableToken(std::string_view(kAbcd)))
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(kExpectedHashOfAbcd));
}

TEST(IdentifiabilityMetricBuilderTest, SetStdString) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiableToken sample((std::string(kAbcd)));
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, sample)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(kExpectedHashOfAbcd));
}

TEST(IdentifiabilityMetricBuilderTest, SetInt) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, -5)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(-5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetIntRef) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  int x = -5;
  int& xref = x;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, xref)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(-5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetIntConstRef) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  int x = -5;
  const int& xref = x;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, xref)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(-5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetUnsigned) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, 5u)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetUint64) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, UINT64_C(5))
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(5)));
}

TEST(IdentifiabilityMetricBuilderTest, SetBigUnsignedInt) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  // Slightly different in that this value cannot be converted into the sample
  // type without loss. Hence it is digested as raw bytes.
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, std::numeric_limits<uint64_t>::max())
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(-1)));
}

TEST(IdentifiabilityMetricBuilderTest, SetFloat) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, 1.5f)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(kExpectedHashOfOnePointFive));
}

TEST(IdentifiabilityMetricBuilderTest, SetDouble) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, 1.5l)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(kExpectedHashOfOnePointFive));
}

TEST(IdentifiabilityMetricBuilderTest, SetEnum) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, Never::kUp)
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(3)));
}

TEST(IdentifiabilityMetricBuilderTest, SetParameterPack) {
  test::ScopedIdentifiabilityTestSampleCollector collector;
  test::TestUkmRecorder recorder;
  IdentifiabilityMetricBuilder(ukm::SourceIdObj{})
      .Add(kTestSurface, IdentifiableToken(1, 2, 3.0, 4, 'a'))
      .Record(&recorder);
  EXPECT_THAT(collector, FirstMetricIs(INT64_C(0x672cf4c107b5b22)));
}

}  // namespace blink
