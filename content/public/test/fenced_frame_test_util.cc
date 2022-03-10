// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fenced_frame_test_util.h"

#include "base/trace_event/typed_macros.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/test/fenced_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {
namespace test {
namespace {

constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    document.body.appendChild(fenced_frame);
  })";

constexpr char kAddAndNavigateFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.src = $1;
    document.body.appendChild(fenced_frame);
  })";

constexpr char kNavigateFrameScript[] = R"({location.href = $1;})";

}  // namespace

FencedFrameTestHelper::FencedFrameTestHelper() {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
}

FencedFrameTestHelper::~FencedFrameTestHelper() = default;

RenderFrameHost* FencedFrameTestHelper::CreateFencedFrame(
    RenderFrameHost* fenced_frame_parent,
    const GURL& url,
    net::Error expected_error_code) {
  TRACE_EVENT("test", "FencedFrameTestHelper::CreateAndGetFencedFrame",
              "fenced_frame_parent", fenced_frame_parent, "url", url);
  RenderFrameHostImpl* fenced_frame_parent_rfh =
      static_cast<RenderFrameHostImpl*>(fenced_frame_parent);

  size_t previous_fenced_frame_count =
      fenced_frame_parent_rfh->GetFencedFrames().size();

  EXPECT_TRUE(ExecJs(fenced_frame_parent_rfh, JsReplace(kAddFencedFrameScript),
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  std::vector<FencedFrame*> fenced_frames =
      fenced_frame_parent_rfh->GetFencedFrames();
  EXPECT_EQ(previous_fenced_frame_count + 1, fenced_frames.size());

  FencedFrame* fenced_frame = fenced_frames.back();
  // It is possible that we got the did stop loading notification because the
  // fenced frame was actually being destroyed. Check to make sure that's not
  // the case. TODO(crbug.com/1123606): Consider weakly referencing the fenced
  // frame if the removal-and-stop-loading scenario is a useful one to test.
  EXPECT_EQ(previous_fenced_frame_count + 1,
            fenced_frame_parent_rfh->GetFencedFrames().size());
  return NavigateFrameInFencedFrameTree(fenced_frame->GetInnerRoot(), url,
                                        expected_error_code);
}

void FencedFrameTestHelper::CreateFencedFrameAsync(
    RenderFrameHost* fenced_frame_parent_rfh,
    const GURL& url) {
  EXPECT_TRUE(ExecJs(fenced_frame_parent_rfh,
                     JsReplace(kAddAndNavigateFencedFrameScript, url),
                     EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

RenderFrameHost* FencedFrameTestHelper::NavigateFrameInFencedFrameTree(
    RenderFrameHost* rfh,
    const GURL& url,
    net::Error expected_error_code) {
  TRACE_EVENT("test", "FencedFrameTestHelper::NavigateFrameInsideFencedFrame",
              "rfh", rfh, "url", url);
  // TODO(domfarolino): Consider adding |url| to the relevant
  // `FencedFrameURLMapping` and then actually passing in the urn:uuid to the
  // script below, so that we exercise the "real" navigation path.

  FrameTreeNode* target_node =
      static_cast<RenderFrameHostImpl*>(rfh)->frame_tree_node();

  TestFrameNavigationObserver fenced_frame_observer(rfh);
  EXPECT_EQ(url.spec(), EvalJs(rfh, JsReplace(kNavigateFrameScript, url)));
  fenced_frame_observer.Wait();

  EXPECT_EQ(target_node->current_frame_host()->IsErrorDocument(),
            expected_error_code != net::OK);

  return target_node->current_frame_host();
}

}  // namespace test

}  // namespace content
