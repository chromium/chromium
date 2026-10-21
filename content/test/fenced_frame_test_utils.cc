// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fenced_frame_test_utils.h"

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "third_party/blink/public/common/features.h"

namespace content {

FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node) {
  FrameTreeNodeId inner_node_id =
      node->current_frame_host()->inner_tree_main_frame_tree_node_id();
  return FrameTreeNode::GloballyFindByID(inner_node_id);
}

TestFencedFrameURLMappingResultObserver::
    TestFencedFrameURLMappingResultObserver() = default;

TestFencedFrameURLMappingResultObserver::
    ~TestFencedFrameURLMappingResultObserver() = default;

void TestFencedFrameURLMappingResultObserver::OnFencedFrameURLMappingComplete(
    const std::optional<FencedFrameProperties>& properties) {
  mapping_complete_observed_ = true;
  observed_fenced_frame_properties_ = properties;
}

void FencedFrameURLMappingTestPeer::FillMap(const GURL& url) {
  while (!fenced_frame_url_mapping_->IsFull()) {
    auto it = fenced_frame_url_mapping_->AddMappingForUrl(url);
    DCHECK(it.has_value());
  }

  DCHECK(fenced_frame_url_mapping_->IsFull());
}

bool PollUntilEvalToTrue(const std::string& script, RenderFrameHost* rfh) {
  base::Time start_time = base::Time::Now();
  base::TimeDelta timeout = TestTimeouts::action_max_timeout();

  while (base::Time::Now() - start_time < timeout) {
    EvalJsResult result = EvalJs(rfh, script);

    if (!result.is_ok()) {
      return false;
    } else if (result.ExtractBool()) {
      return true;
    }

    // Wait for a bit here to keep this loop from spinlocking too badly.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  return false;
}

}  // namespace content
