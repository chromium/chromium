// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_group.h"

#include "base/memory/scoped_refptr.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using SiteInstanceGroupTest = testing::Test;

// Check that a SiteInstanceGroup's BrowsingInstance outlives the
// SiteInstanceGroups within it.
TEST_F(SiteInstanceGroupTest, BrowsingInstanceLifetime) {
  BrowserTaskEnvironment environment;
  TestBrowserContext browser_context;
  std::unique_ptr<MockRenderProcessHost> process =
      std::make_unique<MockRenderProcessHost>(&browser_context);
  scoped_refptr<SiteInstanceGroup> group = nullptr;
  BrowsingInstanceId browsing_instance_id;
  {
    scoped_refptr<BrowsingInstance> browsing_instance = new BrowsingInstance(
        &browser_context, WebExposedIsolationInfo::CreateNonIsolated(),
        /*is_guest=*/false, /*is_fenced=*/false,
        /*is_fixed_storage_partition=*/false, /*coop_related_group=*/nullptr,
        /*common_coop_origin=*/std::nullopt);
    group = new SiteInstanceGroup(browsing_instance.get(), process.get());
    browsing_instance_id = group->browsing_instance_id();
  }

  // The BrowsingInstanceId is accessed by calling into BrowsingInstance rather
  // than being stored on SiteInstanceGroup. Even though `browsing_instance` has
  // gone out of scope here, it has not been destructed since it is kept alive
  // by a scoped_refptr to it in `group`.
  // Note that if this test fails, and the SiteInstanceGroup does outlive the
  // BrowsingInstance (i.e. in the case SiteInstanceGroup holds
  // `browsing_instance_` as a raw pointer, its value will be nullptr here), it
  // will only fail on an ASAN build. In other builds, it will still pass and be
  // a use-after-free.
  EXPECT_EQ(browsing_instance_id, group->browsing_instance_id());
}

// Make sure that it is safe for observers to be deleted while iterating over
// SiteInstanceGroup's observer list.
TEST_F(SiteInstanceGroupTest, ObserverDestructionDuringIteration) {
  static int frame_count_is_zero_calls = 0;
  class TestSiteInstanceGroupObserver : public SiteInstanceGroup::Observer {
   public:
    TestSiteInstanceGroupObserver(SiteInstanceGroup* group) {
      group->AddObserver(this);
    }

    void ActiveFrameCountIsZero(SiteInstanceGroup* group) override {
      group->RemoveObserver(this);
      frame_count_is_zero_calls++;
    }
  };

  BrowserTaskEnvironment environment;
  TestBrowserContext browser_context;
  std::unique_ptr<MockRenderProcessHost> process =
      std::make_unique<MockRenderProcessHost>(&browser_context);
  scoped_refptr<SiteInstanceGroup> group =
      SiteInstanceGroup::CreateForTesting(&browser_context, process.get());

  TestSiteInstanceGroupObserver observer1(group.get());
  TestSiteInstanceGroupObserver observer2(group.get());

  group->IncrementActiveFrameCount();

  // When `active_frame_count_` becomes 0, observers are notified. These
  // observers will delete themselves when that gets called, and will change the
  // state of the observer list when they do so.
  group->DecrementActiveFrameCount();
  EXPECT_EQ(frame_count_is_zero_calls, 2);
}

}  // namespace content
