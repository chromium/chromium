// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"

#include "base/memory/raw_ptr.h"
#include "base/test/power_monitor_test.h"
#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/peer_connection_tracker_host_observer.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom.h"

namespace content {

namespace {

class MockPeerConnectionTrackerHostObserver
    : public PeerConnectionTrackerHostObserver {
 public:
  MockPeerConnectionTrackerHostObserver() = default;
  ~MockPeerConnectionTrackerHostObserver() override = default;

  void OnPeerConnectionAdded(GlobalRenderFrameHostId,
                             int,
                             base::ProcessId,
                             const std::string&,
                             const std::string&) override {}

  void OnPeerConnectionRemoved(GlobalRenderFrameHostId frame_id,
                               int lid) override {
    last_removed_frame_id_ = frame_id;
    last_removed_lid_ = lid;
  }

  void OnPeerConnectionUpdated(GlobalRenderFrameHostId,
                               int,
                               const std::string&,
                               const std::string&) override {}
  void OnPeerConnectionSessionIdSet(GlobalRenderFrameHostId,
                                    int,
                                    const std::string&) override {}
  void OnWebRtcEventLogWrite(GlobalRenderFrameHostId,
                             int,
                             const std::string&) override {}
  void OnWebRtcDataChannelLogWrite(GlobalRenderFrameHostId,
                                   int,
                                   const std::string&) override {}
  void OnAddStandardStats(GlobalRenderFrameHostId,
                          int,
                          base::ListValue) override {}
  void OnGetUserMedia(GlobalRenderFrameHostId,
                      base::ProcessId,
                      int,
                      bool,
                      bool,
                      const std::string&,
                      const std::string&) override {}
  void OnGetUserMediaSuccess(GlobalRenderFrameHostId,
                             base::ProcessId,
                             int,
                             const std::string&,
                             const std::string&,
                             const std::string&) override {}
  void OnGetUserMediaFailure(GlobalRenderFrameHostId,
                             base::ProcessId,
                             int,
                             const std::string&,
                             const std::string&) override {}
  void OnGetDisplayMedia(GlobalRenderFrameHostId,
                         base::ProcessId,
                         int,
                         bool,
                         bool,
                         const std::string&,
                         const std::string&) override {}
  void OnGetDisplayMediaSuccess(GlobalRenderFrameHostId,
                                base::ProcessId,
                                int,
                                const std::string&,
                                const std::string&,
                                const std::string&) override {}
  void OnGetDisplayMediaFailure(GlobalRenderFrameHostId,
                                base::ProcessId,
                                int,
                                const std::string&,
                                const std::string&) override {}

  GlobalRenderFrameHostId last_removed_frame_id_;
  int last_removed_lid_ = -1;
};

}  // namespace

class PeerConnectionTrackerHostTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    if (!base::PowerMonitor::GetInstance()) {
      power_monitor_source_ =
          std::make_unique<base::test::ScopedPowerMonitorTestSource>();
    } else if (!base::PowerMonitor::GetInstance()->IsInitialized()) {
      power_monitor_source_ =
          std::make_unique<base::test::ScopedPowerMonitorTestSource>();
    }
    contents_ = std::unique_ptr<WebContents>(CreateTestWebContents());
  }

  void TearDown() override {
    contents_.reset();
    power_monitor_source_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<base::test::ScopedPowerMonitorTestSource>
      power_monitor_source_;
  std::unique_ptr<WebContents> contents_;
};

TEST_F(PeerConnectionTrackerHostTest, OnDestructorNotifiesObserver) {
  MockPeerConnectionTrackerHostObserver observer;
  NavigateAndCommit(GURL("about:blank"));
  RenderFrameHost* rfh = contents_->GetPrimaryMainFrame();
  static_cast<TestRenderFrameHost*>(rfh)->InitializeRenderFrameIfNeeded();
  ASSERT_TRUE(rfh);
  ASSERT_TRUE(rfh->GetRemoteInterfaces());
  GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
  PeerConnectionTrackerHost* host =
      PeerConnectionTrackerHost::GetOrCreateForCurrentDocument(rfh);

  auto info = blink::mojom::PeerConnectionInfo::New();
  info->lid = 123;
  static_cast<blink::mojom::PeerConnectionTrackerHost*>(host)
      ->AddPeerConnection(std::move(info));

  contents_.reset();

  EXPECT_EQ(observer.last_removed_frame_id_, rfh_id);
  EXPECT_EQ(observer.last_removed_lid_, 123);
}

}  // namespace content
