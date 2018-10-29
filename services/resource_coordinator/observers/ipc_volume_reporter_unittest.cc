// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/ipc_volume_reporter.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

namespace resource_coordinator {

class TestIPCVolumeReporter : public IPCVolumeReporter {
 public:
  TestIPCVolumeReporter()
      : IPCVolumeReporter(std::make_unique<base::MockOneShotTimer>()) {}

  base::MockOneShotTimer* mock_timer() const {
    return static_cast<base::MockOneShotTimer*>(timer());
  }
};

class IPCVolumeReporterTest : public CoordinationUnitTestHarness {
 public:
  IPCVolumeReporterTest() : CoordinationUnitTestHarness() {}

  void SetUp() override {
    reporter_ = new TestIPCVolumeReporter();
    coordination_unit_graph()->RegisterObserver(base::WrapUnique(reporter_));
  }

 protected:
  TestIPCVolumeReporter* reporter_;
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IPCVolumeReporterTest);
};

TEST_F(IPCVolumeReporterTest, Basic) {
  EXPECT_TRUE(reporter_->mock_timer()->IsRunning());
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph(
      coordination_unit_graph());

  cu_graph.frame->SetAudibility(true);
  cu_graph.frame->SetNetworkAlmostIdle(true);
  cu_graph.frame->OnAlertFired();
  cu_graph.frame->OnNonPersistentNotificationCreated();

  cu_graph.page->SetVisibility(true);
  cu_graph.page->SetUKMSourceId(1);
  cu_graph.page->OnFaviconUpdated();
  cu_graph.page->OnTitleUpdated();
  cu_graph.page->OnMainFrameNavigationCommitted(
      ResourceCoordinatorClock::NowTicks(), 1u, "http://example.org");

  cu_graph.process->SetCPUUsage(1.0);
  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));
  cu_graph.process->SetLaunchTime(base::Time());
  cu_graph.process->SetMainThreadTaskLoadIsLow(true);

  reporter_->mock_timer()->Fire();

  histogram_tester_.ExpectTotalCount("ResourceCoordinator.IPCPerMinute.Frame",
                                     1);
  histogram_tester_.ExpectUniqueSample("ResourceCoordinator.IPCPerMinute.Frame",
                                       4, 1);
  histogram_tester_.ExpectTotalCount("ResourceCoordinator.IPCPerMinute.Page",
                                     1);
  histogram_tester_.ExpectUniqueSample("ResourceCoordinator.IPCPerMinute.Page",
                                       5, 1);
  histogram_tester_.ExpectTotalCount("ResourceCoordinator.IPCPerMinute.Process",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "ResourceCoordinator.IPCPerMinute.Process", 4, 1);

  EXPECT_TRUE(reporter_->mock_timer()->IsRunning());
};

}  // namespace resource_coordinator
