// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_content_client_initializer.h"

#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/mock_agent_scheduling_group_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_view_host_factory.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace content {

TestContentClientInitializer::TestContentClientInitializer() {
  test_network_connection_tracker_ =
      network::TestNetworkConnectionTracker::CreateInstance();
  SetNetworkConnectionTrackerForTesting(
      network::TestNetworkConnectionTracker::GetInstance());

  content_client_ = std::make_unique<TestContentClient>();
  SetContentClient(content_client_.get());

  content_browser_client_ = std::make_unique<TestContentBrowserClient>();
  content::SetBrowserClientForTesting(content_browser_client_.get());

  browser_accessibility_state_ = BrowserAccessibilityStateImpl::Create();
}

TestContentClientInitializer::~TestContentClientInitializer() {
  browser_accessibility_state_.reset();
  test_render_view_host_factory_.reset();
  rph_factory_.reset();

  SetContentClient(nullptr);
  content_client_.reset();

  content_browser_client_.reset();

  SetNetworkConnectionTrackerForTesting(nullptr);
  test_network_connection_tracker_.reset();
}

void TestContentClientInitializer::CreateTestRenderViewHosts() {
  rph_factory_ = std::make_unique<MockRenderProcessHostFactory>();
  asgh_factory_ = std::make_unique<MockAgentSchedulingGroupHostFactory>();
  test_render_view_host_factory_ = std::make_unique<TestRenderViewHostFactory>(
      rph_factory_.get(), asgh_factory_.get());
}

}  // namespace content
