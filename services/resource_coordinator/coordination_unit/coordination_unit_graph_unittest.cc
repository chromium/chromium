// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {
namespace {

ProcessCoordinationUnitImpl* CreateProcessCU(CoordinationUnitGraph* graph) {
  return graph->CreateProcessCoordinationUnit(
      CoordinationUnitID(CoordinationUnitType::kProcess,
                         CoordinationUnitID::RANDOM_ID),
      nullptr);
}

}  // namespace

TEST(CoordinationUnitGraphTest, DestructionWhileCUSOutstanding) {
  std::unique_ptr<CoordinationUnitGraph> graph(new CoordinationUnitGraph());

  for (size_t i = 0; i < 10; ++i) {
    ProcessCoordinationUnitImpl* process = CreateProcessCU(graph.get());
    EXPECT_NE(nullptr, process);

    process->SetPID(i + 100);
  }

  EXPECT_NE(nullptr, graph->FindOrCreateSystemCoordinationUnit(nullptr));

  // This should destroy all the CUs without incident.
  graph.reset();
}

TEST(CoordinationUnitGraphTest, FindOrCreateSystemCoordinationUnit) {
  CoordinationUnitGraph graph;

  SystemCoordinationUnitImpl* system_cu =
      graph.FindOrCreateSystemCoordinationUnit(nullptr);

  // A second request should return the same instance.
  EXPECT_EQ(system_cu, graph.FindOrCreateSystemCoordinationUnit(nullptr));

  // Destructing the system CU should be allowed.
  system_cu->Destruct();

  system_cu = graph.FindOrCreateSystemCoordinationUnit(nullptr);
  EXPECT_NE(nullptr, system_cu);
}

TEST(CoordinationUnitGraphTest, GetProcessCoordinationUnitByPid) {
  CoordinationUnitGraph graph;

  ProcessCoordinationUnitImpl* process = CreateProcessCU(&graph);
  EXPECT_EQ(base::kNullProcessId, process->process_id());

  static constexpr base::ProcessId kPid = 10;

  EXPECT_EQ(nullptr, graph.GetProcessCoordinationUnitByPid(kPid));
  process->SetPID(kPid);
  EXPECT_EQ(process, graph.GetProcessCoordinationUnitByPid(kPid));

  process->Destruct();

  EXPECT_EQ(nullptr, graph.GetProcessCoordinationUnitByPid(12));
}

TEST(CoordinationUnitGraphTest, PIDReuse) {
  // This test emulates what happens on Windows under aggressive PID reuse,
  // where a process termination notification can be delayed until after the
  // PID has been reused for a new process.
  CoordinationUnitGraph graph;

  static constexpr base::ProcessId kPid = 10;

  ProcessCoordinationUnitImpl* process1 = CreateProcessCU(&graph);
  ProcessCoordinationUnitImpl* process2 = CreateProcessCU(&graph);

  process1->SetPID(kPid);
  EXPECT_EQ(process1, graph.GetProcessCoordinationUnitByPid(kPid));

  // The second registration for the same PID should override the first one.
  process2->SetPID(kPid);
  EXPECT_EQ(process2, graph.GetProcessCoordinationUnitByPid(kPid));

  // The destruction of the first process CU shouldn't clear the PID
  // registration.
  process1->Destruct();
  EXPECT_EQ(process2, graph.GetProcessCoordinationUnitByPid(kPid));
}

TEST(CoordinationUnitGraphTest, GetAllCUsByType) {
  CoordinationUnitGraph graph;
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph(&graph);

  std::vector<ProcessCoordinationUnitImpl*> processes =
      graph.GetAllProcessCoordinationUnits();
  ASSERT_EQ(1u, processes.size());
  EXPECT_NE(nullptr, processes[0]);

  std::vector<FrameCoordinationUnitImpl*> frames =
      graph.GetAllFrameCoordinationUnits();
  ASSERT_EQ(2u, frames.size());
  EXPECT_NE(nullptr, frames[0]);
  EXPECT_NE(nullptr, frames[1]);

  std::vector<PageCoordinationUnitImpl*> pages =
      graph.GetAllPageCoordinationUnits();
  ASSERT_EQ(2u, pages.size());
  EXPECT_NE(nullptr, pages[0]);
  EXPECT_NE(nullptr, pages[1]);
}

}  // namespace resource_coordinator
