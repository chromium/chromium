// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_HIT_TEST_REGION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_HIT_TEST_REGION_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/hit_test/hit_test_region_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;

// Waits until hit test data for |child_frame| or |guest_web_contents| has been
// submitted, see HitTestRegionObserver::WaitForHitTestData().
void WaitForHitTestData(RenderFrameHost* child_frame);
void WaitForHitTestData(WebContents* guest_web_contents);

// TODO(jonross): Move this to components/viz/host/hit_test/ as a standalone
// HitTestDataWaiter (is-a HitTestRegionObserver) once Viz HitTesting is on by
// default, and there are no longer dependancies upon content.
//
// Test API which observes the arrival / change of hit test data within a Viz
// host.
//
// HitTestRegionObserver is bound to a viz::FrameSinkId for which it observers
// changes in hit test data.
class HitTestRegionObserver : public viz::HitTestRegionObserver {
 public:
  explicit HitTestRegionObserver(const viz::FrameSinkId& frame_sink_id);
  ~HitTestRegionObserver() override;

  // The following functions need to be called in order to wait for the change
  // in hit test data. The first one should be called before any potential
  // change to hit test data (to cache the current state) and the second one
  // should be called after the change. Note that if any change has occurred
  // after the call to WaitForHitTestData, WaitForHitTestDataChange will return
  // immediately and the desired data may not be returned. Looping until the
  // received data match the expected data should be useful in such case.
  void WaitForHitTestData();
  void WaitForHitTestDataChange();
  const std::vector<viz::AggregatedHitTestRegion>& GetHitTestData();

 private:
  // viz::HitTestRegionObserver:
  void OnAggregatedHitTestRegionListUpdated(
      const viz::FrameSinkId& frame_sink_id,
      const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) override;

  viz::FrameSinkId const frame_sink_id_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<base::RunLoop> hit_test_data_change_run_loop_;
  std::vector<viz::AggregatedHitTestRegion> cached_hit_test_data_;

  DISALLOW_COPY_AND_ASSIGN(HitTestRegionObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_HIT_TEST_REGION_OBSERVER_H_
