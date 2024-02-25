// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_permission_checker.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "url/origin.h"

using blink::mojom::MediaDeviceType;

namespace content {

namespace {

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  ~TestWebContentsDelegate() override {}

  bool CheckMediaAccessPermission(RenderFrameHost* render_Frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    return true;
  }
};

}  // namespace

class MediaDevicesPermissionCheckerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(origin_.GetURL());
    contents()->SetDelegate(&delegate_);
  }

 protected:
  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(
      blink::mojom::PermissionsPolicyFeature feature,
      bool enabled) {
    auto navigation = NavigationSimulator::CreateBrowserInitiated(
        origin_.GetURL(), web_contents());
    std::vector<blink::OriginWithPossibleWildcards> allowlist;
    if (enabled) {
      allowlist.emplace_back(
          *blink::OriginWithPossibleWildcards::FromOrigin(origin_));
    }
    navigation->SetPermissionsPolicyHeader({{feature, allowlist,
                                             /*self_if_matches=*/std::nullopt,
                                             /*matches_all_origins=*/false,
                                             /*matches_opaque_src=*/false}});
    navigation->Commit();
  }

  bool CheckPermission(MediaDeviceType device_type) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    checker_.CheckPermission(
        device_type, main_rfh()->GetProcess()->GetID(),
        main_rfh()->GetRoutingID(),
        base::BindOnce(
            &MediaDevicesPermissionCheckerTest::CheckPermissionCallback,
            base::Unretained(this)));
    run_loop.Run();

    EXPECT_FALSE(quit_closure_);  // It was Run() via CheckPermissionCallback().
    return callback_result_;
  }

 private:
  void CheckPermissionCallback(bool result) {
    callback_result_ = result;
    std::move(quit_closure_).Run();
  }

  url::Origin origin_ = url::Origin::Create(GURL("https://www.google.com"));

  base::OnceClosure quit_closure_;
  bool callback_result_ = false;

  MediaDevicesPermissionChecker checker_;
  TestWebContentsDelegate delegate_;
};

// Basic tests for permissions policy checks through the
// MediaDevicesPermissionChecker.  These tests are not meant to cover every edge
// case as the PermissionsPolicy class itself is tested thoroughly in
// permissions_policy_unittest.cc and in
// render_frame_host_permissions_policy_unittest.cc.
TEST_F(MediaDevicesPermissionCheckerTest,
       CheckPermissionWithPermissionsPolicy) {
  // Mic and Camera should be enabled by default for a frame (if permission is
  // granted).
  EXPECT_TRUE(CheckPermission(MediaDeviceType::kMediaAudioInput));
  EXPECT_TRUE(CheckPermission(MediaDeviceType::kMediaVideoInput));

  RefreshPageAndSetHeaderPolicy(
      blink::mojom::PermissionsPolicyFeature::kMicrophone,
      /*enabled=*/false);
  EXPECT_FALSE(CheckPermission(MediaDeviceType::kMediaAudioInput));
  EXPECT_TRUE(CheckPermission(MediaDeviceType::kMediaVideoInput));

  RefreshPageAndSetHeaderPolicy(blink::mojom::PermissionsPolicyFeature::kCamera,
                                /*enabled=*/false);
  EXPECT_TRUE(CheckPermission(MediaDeviceType::kMediaAudioInput));
  EXPECT_FALSE(CheckPermission(MediaDeviceType::kMediaVideoInput));
}

}  // namespace
