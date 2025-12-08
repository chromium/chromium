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
       WindowsToExcludeReturnsEmptyWhenNoPip) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr,
      /*initial_pip_window_id=*/std::nullopt, owner_render_frame_host_id,
      captures);

  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  EXPECT_TRUE(proxy->WindowsToExclude(media_id).empty());
}

TEST_F(PipScreenCaptureCoordinatorProxyImplTest,
       WindowsToExcludeReturnsPipWindowWhenNoCaptures) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures;
  const NativeWindowId pip_window_id = 123;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr, pip_window_id, owner_render_frame_host_id, captures);

  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  std::vector<NativeWindowId> excluded_windows =
      proxy->WindowsToExclude(media_id);
  ASSERT_EQ(1u, excluded_windows.size());
  EXPECT_EQ(pip_window_id, excluded_windows[0]);
}

TEST_F(PipScreenCaptureCoordinatorProxyImplTest,
       WindowsToExcludeReturnsPipWindowForOwnedCapture) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures = {
      {base::UnguessableToken::Create(), owner_render_frame_host_id, media_id}};
  const NativeWindowId pip_window_id = 123;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr, pip_window_id, owner_render_frame_host_id, captures);

  std::vector<NativeWindowId> excluded_windows =
      proxy->WindowsToExclude(media_id);
  ASSERT_EQ(1u, excluded_windows.size());
  EXPECT_EQ(pip_window_id, excluded_windows[0]);
}

TEST_F(PipScreenCaptureCoordinatorProxyImplTest,
       WindowsToExcludeReturnsEmptyForUnownedCapture) {
  const GlobalRenderFrameHostId owner_render_frame_host_id(1, 1);
  const GlobalRenderFrameHostId other_render_frame_host_id(2, 2);
  const DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, 1);
  const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures = {
      {base::UnguessableToken::Create(), other_render_frame_host_id, media_id}};
  const NativeWindowId pip_window_id = 123;

  auto proxy = std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      nullptr, pip_window_id, owner_render_frame_host_id, captures);

  EXPECT_TRUE(proxy->WindowsToExclude(media_id).empty());
}

}  // namespace content
