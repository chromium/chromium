// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fenced_frame_test_util.h"

#include "base/trace_event/typed_macros.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
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

FencedFrameTestHelper::FencedFrameTestHelper(FencedFrameType type)
    : type_(type) {
  if (type == FencedFrameType::kMPArch) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {{"implementation_type", "mparch"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  } else {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames,
          {{"implementation_type", "shadow_dom"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }
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
  RenderFrameHostImpl* fenced_frame_rfh;
  if (type_ == FencedFrameType::kMPArch) {
    size_t previous_fenced_frame_count =
        fenced_frame_parent_rfh->GetFencedFrames().size();

    EXPECT_TRUE(ExecJs(fenced_frame_parent_rfh,
                       JsReplace(kAddFencedFrameScript),
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
    fenced_frame_rfh = fenced_frame->GetInnerRoot();
  } else {
    EXPECT_TRUE(ExecJs(fenced_frame_parent_rfh,
                       JsReplace(kAddFencedFrameScript),
                       EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
    fenced_frame_rfh = static_cast<RenderFrameHostImpl*>(
        fenced_frame_parent_rfh
            ->child_at(fenced_frame_parent_rfh->child_count() - 1)
            ->current_frame_host());
  }
  if (url.is_empty())
    return fenced_frame_rfh;
  return NavigateFrameInFencedFrameTree(fenced_frame_rfh, url,
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

// static
RenderFrameHost* FencedFrameTestHelper::GetMostRecentlyAddedFencedFrame(
    RenderFrameHost* rfh) {
  if (blink::features::kFencedFramesImplementationTypeParam.Get() ==
      blink::features::FencedFramesImplementationType::kMPArch) {
    std::vector<FencedFrame*> fenced_frames =
        static_cast<RenderFrameHostImpl*>(rfh)->GetFencedFrames();
    if (fenced_frames.empty())
      return nullptr;
    return fenced_frames.back()->GetInnerRoot();
  }
  if (blink::features::kFencedFramesImplementationTypeParam.Get() ==
      blink::features::FencedFramesImplementationType::kShadowDOM) {
    RenderFrameHostImpl* parent_rfh = static_cast<RenderFrameHostImpl*>(rfh);
    for (size_t i = 0, ff_index = parent_rfh->child_count() - 1;
         i < parent_rfh->child_count(); ++i, --ff_index) {
      RenderFrameHost* child_rfh =
          parent_rfh->child_at(ff_index)->current_frame_host();
      if (child_rfh->IsFencedFrameRoot())
        return child_rfh;
    }
    return nullptr;
  }
  return nullptr;
}

GURL CreateFencedFrameURLMapping(RenderFrameHost* rfh, const GURL& url) {
  FrameTreeNode* target_node =
      static_cast<RenderFrameHostImpl*>(rfh)->frame_tree_node();
  FencedFrameURLMapping& url_mapping =
      target_node->current_frame_host()->GetPage().fenced_frame_urls_map();
  GURL urn_uuid = url_mapping.AddFencedFrameURL(url);
  EXPECT_TRUE(urn_uuid.is_valid());
  return urn_uuid;
}
}  // namespace test

}  // namespace content
