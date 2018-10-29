// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/webui_graph_dump_impl.h"

#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class WebUIGraphDumpImplTest : public CoordinationUnitTestHarness {};

TEST_F(WebUIGraphDumpImplTest, Create) {
  CoordinationUnitGraph graph;
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph(&graph);

  base::TimeTicks now = ResourceCoordinatorClock::NowTicks();

  constexpr char kExampleUrl[] = "http://www.example.org";
  cu_graph.page->OnMainFrameNavigationCommitted(now, 1, kExampleUrl);
  cu_graph.other_page->OnMainFrameNavigationCommitted(now, 2, kExampleUrl);

  WebUIGraphDumpImpl impl(&graph);

  mojom::WebUIGraphPtr returned_graph;
  WebUIGraphDumpImpl::GetCurrentGraphCallback callback =
      base::BindLambdaForTesting([&returned_graph](mojom::WebUIGraphPtr graph) {
        returned_graph = std::move(graph);
      });
  impl.GetCurrentGraph(std::move(callback));

  task_env().RunUntilIdle();

  ASSERT_NE(nullptr, returned_graph.get());
  EXPECT_EQ(2u, returned_graph->pages.size());
  for (const auto& page : returned_graph->pages) {
    EXPECT_NE(0u, page->id);
    EXPECT_NE(0u, page->main_frame_id);
  }

  EXPECT_EQ(3u, returned_graph->frames.size());
  // Count the top-level frames as we go.
  size_t top_level_frames = 0;
  for (const auto& frame : returned_graph->frames) {
    if (frame->parent_frame_id == 0)
      ++top_level_frames;
    EXPECT_NE(0u, frame->id);
    EXPECT_NE(0u, frame->process_id);
  }
  // Make sure we have one top-level frame per page.
  EXPECT_EQ(returned_graph->pages.size(), top_level_frames);

  EXPECT_EQ(2u, returned_graph->processes.size());
  for (const auto& page : returned_graph->pages) {
    EXPECT_NE(0u, page->id);
    EXPECT_NE(0u, page->main_frame_id);
    EXPECT_EQ(kExampleUrl, page->main_frame_url);
  }
}

}  // namespace resource_coordinator
