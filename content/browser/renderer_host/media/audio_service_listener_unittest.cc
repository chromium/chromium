// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using RunningServiceInfoPtr = service_manager::mojom::RunningServiceInfoPtr;

namespace content {

namespace {

RunningServiceInfoPtr MakeTestServiceInfo(
    const service_manager::Identity& identity,
    uint32_t pid) {
  RunningServiceInfoPtr info(service_manager::mojom::RunningServiceInfo::New());
  info->identity = identity;
  info->pid = pid;
  return info;
}

}  // namespace

struct AudioServiceListenerMetricsTest : public testing::Test {
  AudioServiceListenerMetricsTest() {
    test_clock.SetNowTicks(base::TimeTicks::Now());
  }

  base::SimpleTestTickClock test_clock;
  base::HistogramTester histogram_tester;
};

TEST_F(AudioServiceListenerMetricsTest,
       ServiceCreatedStartedStopped_LogsStartupTime_LogsUptime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceCreated();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(42));
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount(
      "Media.AudioService.ObservedStartupTime",
      base::TimeDelta::FromMilliseconds(42), 1);
  test_clock.Advance(base::TimeDelta::FromDays(2));
  metrics.ServiceStopped();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedUptime",
                                         base::TimeDelta::FromDays(2), 1);

  test_clock.Advance(base::TimeDelta::FromHours(5));
  metrics.ServiceCreated();
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedDowntime2",
                                         base::TimeDelta::FromHours(5), 1);
}

TEST_F(AudioServiceListenerMetricsTest,
       CreateMetricsStartService_LogsInitialDowntime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  test_clock.Advance(base::TimeDelta::FromHours(12));
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount(
      "Media.AudioService.ObservedInitialDowntime",
      base::TimeDelta::FromHours(12), 1);
}

TEST_F(AudioServiceListenerMetricsTest,
       ServiceAlreadyRunningStopService_LogsUptime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceAlreadyRunning();
  test_clock.Advance(base::TimeDelta::FromMinutes(42));
  metrics.ServiceStopped();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedUptime",
                                         base::TimeDelta::FromMinutes(42), 1);
}

TEST_F(AudioServiceListenerMetricsTest,
       ServiceAlreadyRunningCreateService_LogsStartupTime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceAlreadyRunning();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(2));
  metrics.ServiceCreated();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(20));
  metrics.ServiceStarted();
  histogram_tester.ExpectTimeBucketCount(
      "Media.AudioService.ObservedStartupTime",
      base::TimeDelta::FromMilliseconds(20), 1);
}

// Check that if service was already started and ServiceStarted() is called,
// ObservedStartupTime and ObservedInitialDowntime are not logged and start time
// is reset.
TEST_F(AudioServiceListenerMetricsTest,
       ServiceAlreadyRunningStartService_ResetStartTime) {
  AudioServiceListener::Metrics metrics(&test_clock);
  metrics.ServiceAlreadyRunning();
  test_clock.Advance(base::TimeDelta::FromMilliseconds(20));
  metrics.ServiceStarted();
  histogram_tester.ExpectTotalCount("Media.AudioService.ObservedStartupTime",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Media.AudioService.ObservedInitialDowntime", 0);
  test_clock.Advance(base::TimeDelta::FromMilliseconds(200));
  metrics.ServiceStopped();
  histogram_tester.ExpectTimeBucketCount("Media.AudioService.ObservedUptime",
                                         base::TimeDelta::FromMilliseconds(200),
                                         1);
}

TEST(AudioServiceListenerTest, StartService_LogStartStatus) {
  base::HistogramTester histogram_tester;
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity audio_service_identity(audio::mojom::kServiceName);
  constexpr base::ProcessId pid(42);

  std::vector<RunningServiceInfoPtr> instances;
  instances.push_back(MakeTestServiceInfo(audio_service_identity, pid));
  audio_service_listener.OnInit(std::move(instances));
  histogram_tester.ExpectBucketCount(
      "Media.AudioService.ObservedStartStatus",
      static_cast<int>(
          AudioServiceListener::Metrics::ServiceStartStatus::kAlreadyStarted),
      1);
  audio_service_listener.OnServiceStopped(audio_service_identity);

  audio_service_listener.OnServiceCreated(
      MakeTestServiceInfo(audio_service_identity, pid));
  audio_service_listener.OnServiceStarted(audio_service_identity, pid);
  histogram_tester.ExpectBucketCount(
      "Media.AudioService.ObservedStartStatus",
      static_cast<int>(
          AudioServiceListener::Metrics::ServiceStartStatus::kSuccess),
      1);
  audio_service_listener.OnServiceStopped(audio_service_identity);

  audio_service_listener.OnServiceCreated(
      MakeTestServiceInfo(audio_service_identity, pid));
  audio_service_listener.OnServiceFailedToStart(audio_service_identity);
  histogram_tester.ExpectBucketCount(
      "Media.AudioService.ObservedStartStatus",
      static_cast<int>(
          AudioServiceListener::Metrics::ServiceStartStatus::kFailure),
      1);
}

TEST(AudioServiceListenerTest, OnInitWithoutAudioService_ProcessIdNull) {
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity id("id1");
  constexpr base::ProcessId pid(42);
  std::vector<RunningServiceInfoPtr> instances;
  instances.push_back(MakeTestServiceInfo(id, pid));
  audio_service_listener.OnInit(std::move(instances));
  EXPECT_EQ(base::kNullProcessId, audio_service_listener.GetProcessId());
}

TEST(AudioServiceListenerTest, OnInitWithAudioService_ProcessIdNotNull) {
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity audio_service_identity(audio::mojom::kServiceName);
  constexpr base::ProcessId pid(42);
  std::vector<RunningServiceInfoPtr> instances;
  instances.push_back(MakeTestServiceInfo(audio_service_identity, pid));
  audio_service_listener.OnInit(std::move(instances));
  EXPECT_EQ(pid, audio_service_listener.GetProcessId());
}

TEST(AudioServiceListenerTest, OnAudioServiceCreated_ProcessIdNotNull) {
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity audio_service_identity(audio::mojom::kServiceName);
  constexpr base::ProcessId pid(42);
  audio_service_listener.OnServiceCreated(
      MakeTestServiceInfo(audio_service_identity, pid));
  audio_service_listener.OnServiceStarted(audio_service_identity, pid);
  EXPECT_EQ(pid, audio_service_listener.GetProcessId());
}

}  // namespace content
