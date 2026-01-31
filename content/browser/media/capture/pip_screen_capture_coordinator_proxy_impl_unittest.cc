// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy_impl.h"

#include "base/unguessable_token.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PipScreenCaptureCoordinatorProxyImplTest : public testing::Test {
 public:
  PipScreenCaptureCoordinatorProxyImplTest() = default;
  ~PipScreenCaptureCoordinatorProxyImplTest() override = default;

 protected:
  BrowserTaskEnvironment task_environment_;
};

TEST_F(PipScreenCaptureCoordinatorProxyImplTest,
       WindowToExcludeReturnsEmptyWhenNoPip) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr,
      /*initial_pip_window_id=*/std::nullopt, owner_render_frame_host_id,
      captures);

  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  EXPECT_FALSE(proxy->WindowToExclude(media_id).has_value());
}

TEST_F(PipScreenCaptureCoordinatorProxyImplTest,
       WindowToExcludeReturnsPipWindowWhenNoCaptures) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures;
  const DesktopMediaID::Id pip_window_id = 123;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr, pip_window_id, owner_render_frame_host_id, captures);

  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  EXPECT_EQ(proxy->WindowToExclude(media_id), pip_window_id);
}

TEST_F(PipScreenCaptureCoordinatorProxyImplTest,
       WindowToExcludeReturnsPipWindowForOwnedCapture) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures = {
      {base::UnguessableToken::Create(), owner_render_frame_host_id, media_id}};
  const DesktopMediaID::Id pip_window_id = 123;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr, pip_window_id, owner_render_frame_host_id, captures);

  EXPECT_EQ(proxy->WindowToExclude(media_id), pip_window_id);
}

TEST_F(PipScreenCaptureCoordinatorProxyImplTest,
       WindowToExcludeReturnsEmptyForUnownedCapture) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const GlobalRenderFrameHostId other_render_frame_host_id(2, 2);
  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures = {
      {base::UnguessableToken::Create(), other_render_frame_host_id, media_id}};
  const DesktopMediaID::Id pip_window_id = 123;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr, pip_window_id, owner_render_frame_host_id, captures);

  EXPECT_FALSE(proxy->WindowToExclude(media_id).has_value());
}

}  // namespace content
