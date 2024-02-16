// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/privacy_budget/aggregating_sample_collector.h"

#include <memory>
#include <type_traits>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/privacy_budget/identifiability_sample_collector_test_utils.h"
#include "third_party/blink/common/privacy_budget/test_ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_sample.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {
using testing::Pointee;
using testing::UnorderedElementsAre;
}  // namespace

namespace blink {

namespace {
constexpr ukm::SourceId kTestSource1 = 1;
constexpr ukm::SourceId kTestSource2 = 2;
constexpr IdentifiableSurface kTestSurface1 =
    IdentifiableSurface::FromMetricHash(1 << 8);
constexpr IdentifiableSurface kTestSurface2 =
    IdentifiableSurface::FromMetricHash(2 << 8);
constexpr IdentifiableToken kTestValue1 = 1;

// A settings provider that activates the study and allows all surfaces and
// types.
class TestSettingsProvider : public IdentifiabilityStudySettingsProvider {
 public:
  bool IsMetaExperimentActive() const override { return false; }
  bool IsActive() const override { return true; }
  bool IsAnyTypeOrSurfaceBlocked() const override { return false; }
  bool IsSurfaceAllowed(IdentifiableSurface) const override { return true; }
  bool IsTypeAllowed(IdentifiableSurface::Type) const override { return true; }
};

}  // namespace

class AggregatingSampleCollectorTest : public ::testing::Test {
 public:
  AggregatingSampleCollectorTest() {
    IdentifiabilityStudySettings::SetGlobalProvider(
        std::make_unique<TestSettingsProvider>());
  }

  ~AggregatingSampleCollectorTest() override {
    IdentifiabilityStudySettings::ResetStateForTesting();
  }

  test::TestUkmRecorder* recorder() { return &recorder_; }
  AggregatingSampleCollector* collector() { return &collector_; }

  base::test::TaskEnvironment& task_environment() { return environment; }

 protected:
  test::TestUkmRecorder recorder_;
  AggregatingSampleCollector collector_;

  base::test::TaskEnvironment environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AggregatingSampleCollectorTest, NoImmediatePassthrough) {
  std::vector<IdentifiableSample> samples = {{kTestSurface1, kTestValue1}};

  collector()->Record(recorder(), kTestSource1, std::move(samples));
  // Should not have passed along any metrics yet.
  EXPECT_EQ(0u, recorder()->entries_count());

  // And should have done so now.
  collector()->Flush(recorder());
  EXPECT_EQ(1u, recorder()->entries_count());
}

TEST_F(AggregatingSampleCollectorTest, MergesDuplicates) {
  base::HistogramTester histogram_tester;

  std::vector<IdentifiableSample> samples = {{kTestSurface1, kTestValue1}};

  // The same set of samples are recorded repeatedly against different sources.
  // The metrics should be deduplicated per source.
  for (auto i = 0; i < 1000; ++i)
    collector()->Record(recorder(), kTestSource1, samples);
  for (auto i = 0; i < 1000; ++i)
    collector()->Record(recorder(), kTestSource2, samples);
  EXPECT_EQ(0u, recorder()->entries_count());

  collector()->Flush(recorder());
  const auto entries = recorder()->GetEntriesByHash(
      ukm::builders::Identifiability::kEntryNameHash);

  // We end up with two entries, one per source.
  EXPECT_THAT(
      entries,
      UnorderedElementsAre(
          Pointee(*ukm::mojom::UkmEntry::New(
              kTestSource1, ukm::builders::Identifiability::kEntryNameHash,
              base::flat_map<uint64_t, int64_t>{
                  {kTestSurface1.ToUkmMetricHash(),
                   kTestValue1.ToUkmMetricValue()}})),
          Pointee(*ukm::mojom::UkmEntry::New(
              kTestSource2, ukm::builders::Identifiability::kEntryNameHash,
              base::flat_map<uint64_t, int64_t>{
                  {kTestSurface1.ToUkmMetricHash(),
                   kTestValue1.ToUkmMetricValue()}}))));

  histogram_tester.ExpectBucketCount(
      "PrivacyBudget.Identifiability.RecordedSample",
      PrivacyBudgetRecordedSample::kAccepted, 2);
}

TEST_F(AggregatingSampleCollectorTest, DoesNotCountDuplicates) {
  // Similar to the MergesDuplicates test. We record the same value a bunch of
  // times, and then record another value a bunch of times. This should record
  // two values for the same surface.
  const int kValue1 = 1 << 1;
  const int kValue2 = 1 << 2;
  for (auto i = 0; i < 1000; ++i)
    collector()->Record(recorder(), kTestSource1, {{kTestSurface1, kValue1}});
  for (auto i = 0; i < 1000; ++i)
    collector()->Record(recorder(), kTestSource1, {{kTestSurface1, kValue2}});
  // Should not have reported anything.
  EXPECT_EQ(0u, recorder()->entries_count());

  collector()->Flush(recorder());
  const auto entries = recorder()->GetEntriesByHash(
      ukm::builders::Identifiability::kEntryNameHash);

  // We end up with two entries because the observations cannot be represented
  // in a single UkmEntry.
  ASSERT_EQ(2u, entries.size());

  // There's no defined ordering for the two entries since they are reported in
  // the order in which they were found in an unordered_multimap. So we OR the
  // values together to make sure we've seen them all.
  int values = 0;
  const auto* entry = entries[0];
  ASSERT_EQ(1u, entry->metrics.size());
  EXPECT_EQ(kTestSurface1.ToUkmMetricHash(), entry->metrics.begin()->first);
  values |= entry->metrics.begin()->second;

  entry = entries[1];
  ASSERT_EQ(1u, entry->metrics.size());
  EXPECT_EQ(kTestSurface1.ToUkmMetricHash(), entry->metrics.begin()->first);
  values |= entry->metrics.begin()->second;

  EXPECT_EQ(values, kValue1 | kValue2);
}

TEST_F(AggregatingSampleCollectorTest, TooManySurfaces) {
  // Reporting kMaxTrackedSurfaces distinct surfaces should cause the tracker to
  // saturate. After this point, metrics aren't recorded. Only using one source
  // to not conflate source limits with surface limits.

  base::HistogramTester histogram_tester;

  unsigned i = 0;
  for (; i < AggregatingSampleCollector::kMaxTrackedSurfaces; ++i) {
    collector()->Record(recorder(), kTestSource1,
                        {{IdentifiableSurface::FromMetricHash(i << 8), 1}});
  }
  collector()->Flush(recorder());
  // There will be a bunch here. The exact number depends on other factors since
  // each entry can include multiple samples.
  EXPECT_NE(0u, recorder()->entries_count());
  recorder()->Purge();
  EXPECT_EQ(0u, recorder()->entries_count());

  // Adding any more doesn't make a difference.
  collector()->Record(recorder(), kTestSource1,
                      {{IdentifiableSurface::FromMetricHash(i << 8), 1}});

  collector()->Flush(recorder());
  // Nothing get recorded.
  EXPECT_EQ(0u, recorder()->entries_count());

  histogram_tester.ExpectBucketCount(
      "PrivacyBudget.Identifiability.RecordedSample",
      PrivacyBudgetRecordedSample::kAccepted,
      AggregatingSampleCollector::kMaxTrackedSurfaces);
  histogram_tester.ExpectBucketCount(
      "PrivacyBudget.Identifiability.RecordedSample",
      PrivacyBudgetRecordedSample::kDroppedMaxTrackedSurfaces, 1);
}

TEST_F(AggregatingSampleCollectorTest, TooManySources) {
  // Reporting surfaces for kMaxTrackedSources distinct sources should cause the
  // tracker to saturate. After this point, metrics aren't recorded. Only using
  // one surface to not conflate source limits with surface limits.

  base::HistogramTester histogram_tester;

  // Start with 1 because 0 is an invalid source id for UKM.
  unsigned i = 1;
  for (; i < AggregatingSampleCollector::kMaxTrackedSources + 1; ++i) {
    collector()->Record(recorder(), i, {{kTestSurface1, kTestValue1}});
  }
  collector()->Flush(recorder());
  // There will be a bunch here. The exact number depends on other factors since
  // each entry can include multiple samples.
  EXPECT_NE(0u, recorder()->entries_count());
  recorder()->Purge();
  EXPECT_EQ(0u, recorder()->entries_count());

  // Additional sources will be ignored.
  collector()->Record(recorder(), i++, {{kTestSurface2, kTestValue1}});

  collector()->Flush(recorder());
  // Nothing gets recorded.
  EXPECT_EQ(0u, recorder()->entries_count());

  // Flushing one source will make room for one additional source.
  collector()->FlushSource(recorder(), 1);
  collector()->Record(recorder(), i++, {{kTestSurface2, kTestValue1}});
  collector()->Flush(recorder());
  EXPECT_EQ(1u, recorder()->entries_count());
  EXPECT_EQ(1u, recorder()->entries()[0]->metrics.size());

  histogram_tester.ExpectBucketCount(
      "PrivacyBudget.Identifiability.RecordedSample",
      PrivacyBudgetRecordedSample::kAccepted,
      AggregatingSampleCollector::kMaxTrackedSources + 1);
  histogram_tester.ExpectBucketCount(
      "PrivacyBudget.Identifiability.RecordedSample",
      PrivacyBudgetRecordedSample::kDroppedMaxTrackedSources, 1);
}

TEST_F(AggregatingSampleCollectorTest, TooManySamplesPerSurface) {
  base::HistogramTester histogram_tester;

  unsigned i = 0;
  // These values are recorded against a single surface and a single source.
  // Once saturated it won't accept any more values.
  for (;
       i < AggregatingSampleCollector::kMaxTrackedSamplesPerSurfacePerSourceId;
       ++i) {
    collector()->Record(recorder(), kTestSource1, {{kTestSurface1, i}});
  }
  collector()->Flush(recorder());
  EXPECT_EQ(AggregatingSampleCollector::kMaxTrackedSamplesPerSurfacePerSourceId,
            recorder()->entries_count());
  EXPECT_EQ(1u, recorder()->entries()[0]->metrics.size());
  recorder()->Purge();
  EXPECT_EQ(0u, recorder()->entries_count());

  // Any more samples for the same source id won't make a difference.
  collector()->Record(recorder(), kTestSource1, {{kTestSurface1, i++}});
  collector()->Flush(recorder());
  EXPECT_EQ(0u, recorder()->entries_count());
  recorder()->Purge();
  EXPECT_EQ(0u, recorder()->entries_count());

  // However, we can record more samples for another source id.
  collector()->Record(recorder(), kTestSource2, {{kTestSurface1, i++}});
  collector()->Flush(recorder());
  EXPECT_EQ(1u, recorder()->entries_count());
  EXPECT_EQ(1u, recorder()->entries()[0]->metrics.size());
  recorder()->Purge();
  EXPECT_EQ(0u, recorder()->entries_count());

  // Moreover, flushing the source will allow to collect more samples for it.
  collector()->FlushSource(recorder(), kTestSource1);
  collector()->Record(recorder(), kTestSource1, {{kTestSurface1, i++}});
  collector()->Flush(recorder());
  EXPECT_EQ(1u, recorder()->entries_count());
  EXPECT_EQ(1u, recorder()->entries()[0]->metrics.size());

  histogram_tester.ExpectBucketCount(
      "PrivacyBudget.Identifiability.RecordedSample",
      PrivacyBudgetRecordedSample::kAccepted,
      AggregatingSampleCollector::kMaxTrackedSamplesPerSurfacePerSourceId + 2);
  histogram_tester.ExpectBucketCount(
      "PrivacyBudget.Identifiability.RecordedSample",
      PrivacyBudgetRecordedSample::kDroppedMaxTrackedPerSurfacePerSource, 1);
}

TEST_F(AggregatingSampleCollectorTest, TooManyUnsentMetrics) {
  // The test is inconclusive if this condition doesn't hold.
  ASSERT_LT(AggregatingSampleCollector::kMaxUnsentSamples,
            AggregatingSampleCollector::kMaxTrackedSurfaces);

  // Stop one short of the limit.
  unsigned i = 0;
  for (; i < AggregatingSampleCollector::kMaxUnsentSamples; ++i) {
    collector()->Record(recorder(), kTestSource1,
                        {{IdentifiableSurface::FromMetricHash(i << 8), 1}});
  }
  EXPECT_EQ(0u, recorder()->entries_count());

  // Adding one should automatically flush.
  collector()->Record(recorder(), kTestSource1,
                      {{IdentifiableSurface::FromMetricHash(i << 8), 1}});
  EXPECT_NE(0u, recorder()->entries_count());
}

TEST_F(AggregatingSampleCollectorTest, TooManyUnsentSources) {
  // The test is inconclusive if this condition doesn't hold.
  ASSERT_LT(AggregatingSampleCollector::kMaxUnsentSources,
            AggregatingSampleCollector::kMaxTrackedSurfaces);
  ASSERT_LT(AggregatingSampleCollector::kMaxUnsentSources,
            AggregatingSampleCollector::kMaxUnsentSamples);

  // Stop one short of the limit.
  unsigned i = 0;
  for (; i < AggregatingSampleCollector::kMaxUnsentSources; ++i) {
    collector()->Record(recorder(), ukm::AssignNewSourceId(),
                        {{IdentifiableSurface::FromMetricHash(i << 8), 1}});
  }
  EXPECT_EQ(0u, recorder()->entries_count());

  // Adding one should automatically flush.
  collector()->Record(recorder(), ukm::AssignNewSourceId(),
                      {{IdentifiableSurface::FromMetricHash(i << 8), 1}});
  EXPECT_NE(0u, recorder()->entries_count());
}

TEST_F(AggregatingSampleCollectorTest, UnsentMetricsAreTooOld) {
  collector()->Record(recorder(), kTestSource1, {{kTestSurface1, 1}});
  EXPECT_EQ(0u, recorder()->entries_count());

  task_environment().FastForwardBy(
      AggregatingSampleCollector::kMaxUnsentSampleAge);
  collector()->Record(recorder(), kTestSource1, {{kTestSurface1, 2}});
  EXPECT_NE(0u, recorder()->entries_count());
}

TEST_F(AggregatingSampleCollectorTest, FlushSource) {
  collector()->Record(recorder(), kTestSource1, {{kTestSurface1, 1}});
  collector()->Record(recorder(), kTestSource2, {{kTestSurface2, 1}});
  collector()->FlushSource(recorder(), kTestSource1);

  EXPECT_EQ(1u, recorder()->entries_count());
  EXPECT_EQ(kTestSource1, recorder()->entries().front()->source_id);

  recorder()->Purge();

  collector()->Flush(recorder());
  EXPECT_EQ(1u, recorder()->entries_count());
  EXPECT_EQ(kTestSource2, recorder()->entries().front()->source_id);
}

// This test exercises the global instance. The goal is to make sure that the
// global instance is what we think it is.
TEST_F(AggregatingSampleCollectorTest, GlobalInstance) {
  ResetCollectorInstanceStateForTesting();

  auto* global_collector = IdentifiabilitySampleCollector::Get();
  global_collector->Record(recorder(), kTestSource1, {{kTestSurface1, 1}});
  EXPECT_EQ(0u, recorder()->entries_count());

  global_collector->Flush(recorder());
  EXPECT_NE(0u, recorder()->entries_count());
}

TEST_F(AggregatingSampleCollectorTest, NullRecorder) {
  collector()->Record(recorder(), kTestSource2, {{kTestSurface2, 1}});

  // Shouldn't crash nor affect state.
  collector()->Record(nullptr, kTestSource1, {{kTestSurface1, 1}});
  collector()->FlushSource(nullptr, kTestSource1);
  collector()->FlushSource(nullptr, kTestSource2);
  collector()->Flush(nullptr);

  collector()->Flush(recorder());
  EXPECT_EQ(1u, recorder()->entries_count());
  EXPECT_EQ(kTestSource2, recorder()->entries().front()->source_id);
}

TEST_F(AggregatingSampleCollectorTest, InvalidSourceId) {
  collector()->Record(recorder(), ukm::kInvalidSourceId, {{kTestSurface2, 2}});
  collector()->Flush(recorder());
  EXPECT_EQ(0u, recorder()->entries_count());
}
}  // namespace blink
