// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_navigation_handle.h"

#include "content/public/browser/web_contents.h"

namespace content {

namespace {

uint64_t g_mock_handle_id = 0;

}  // namespace

MockNavigationHandle::MockNavigationHandle() : MockNavigationHandle(nullptr) {}

MockNavigationHandle::MockNavigationHandle(WebContents* web_contents)
    : navigation_id_(++g_mock_handle_id), web_contents_(web_contents) {
}

MockNavigationHandle::MockNavigationHandle(const GURL& url,
                                           RenderFrameHost* render_frame_host)
    : navigation_id_(++g_mock_handle_id),
      url_(url),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      is_in_primary_main_frame_(render_frame_host_
                                    ? render_frame_host_->IsInPrimaryMainFrame()
                                    : true) {
  redirect_chain_.push_back(url);
  runtime_feature_state_context_ = blink::RuntimeFeatureStateContext();
}

MockNavigationHandle::~MockNavigationHandle() = default;

void MockNavigationHandle::SetAuthChallengeInfo(
    const net::AuthChallengeInfo& challenge) {
  auth_challenge_info_ = challenge;
}

}  // namespace content
