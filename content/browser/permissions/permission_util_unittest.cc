// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace content {

class PermissionUtilTest : public ::testing::Test {};

TEST_F(PermissionUtilTest, ToPermissionStatusWithDetails_ContentSetting) {
  PermissionResult result(blink::mojom::PermissionStatus::GRANTED,
                          PermissionStatusSource::UNSPECIFIED,
                          CONTENT_SETTING_ALLOW);
  auto status_with_details = PermissionUtil::ToPermissionStatusWithDetails(
      blink::mojom::PermissionName::GEOLOCATION, result);
  EXPECT_EQ(status_with_details,
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_Precise) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAllowed,
      .precise = PermissionOption::kAllowed};
  PermissionResult result(blink::mojom::PermissionStatus::GRANTED,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED,
                blink::mojom::PermissionDetails::NewGeolocationAccuracy(
                    blink::mojom::GeolocationAccuracy::kPrecise)));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_PreciseDenied) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAllowed,
      .precise = PermissionOption::kDenied};
  PermissionResult result(blink::mojom::PermissionStatus::GRANTED,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED,
                blink::mojom::PermissionDetails::NewGeolocationAccuracy(
                    blink::mojom::GeolocationAccuracy::kApproximate)));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_PreciseAsk) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAllowed,
      .precise = PermissionOption::kAsk};
  PermissionResult precise_result(blink::mojom::PermissionStatus::ASK,
                                  PermissionStatusSource::UNSPECIFIED,
                                  geolocation_setting);
  PermissionResult approximate_result(blink::mojom::PermissionStatus::GRANTED,
                                      PermissionStatusSource::UNSPECIFIED,
                                      geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, precise_result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::ASK, nullptr));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE,
                approximate_result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_DeniedStatus) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kDenied,
      .precise = PermissionOption::kDenied};
  PermissionResult result(blink::mojom::PermissionStatus::DENIED,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::DENIED, nullptr));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::DENIED, nullptr));
}

TEST_F(PermissionUtilTest,
       ToPermissionStatusWithDetails_GeolocationSetting_AskStatus) {
  GeolocationSetting geolocation_setting = {
      .approximate = PermissionOption::kAsk, .precise = PermissionOption::kAsk};
  PermissionResult result(blink::mojom::PermissionStatus::ASK,
                          PermissionStatusSource::UNSPECIFIED,
                          geolocation_setting);
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::ASK, nullptr));
  EXPECT_EQ(PermissionUtil::ToPermissionStatusWithDetails(
                blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, result),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::ASK, nullptr));
}

// The PermissionType <-> PermissionName mappings for the new permission must
// be inverses; a mismatch would resolve queries against the wrong permission.
TEST_F(PermissionUtilTest, RemoteClientDataJsonDescriptorRoundTrip) {
  blink::mojom::PermissionDescriptorPtr descriptor =
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::
              PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON);
  EXPECT_EQ(descriptor->name,
            blink::mojom::PermissionName::
                PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON);
  EXPECT_EQ(
      blink::PermissionDescriptorToPermissionType(descriptor),
      blink::PermissionType::PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON);
}

class PermissionUtilGetLastCommittedOriginAsURLTest
    : public RenderViewHostTestHarness {
 protected:
  // Navigates the primary main frame of `web_contents()` to `url`, served with
  // a `Content-Security-Policy: sandbox` header, so that the committed document
  // has an opaque origin whose precursor is the origin of `url`.
  RenderFrameHost* NavigateAndCommitSandboxed(const GURL& url) {
    auto navigation =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());
    navigation->SetResponseHeaders(net::HttpResponseHeaders::TryToCreate(
        "HTTP/1.1 200 OK\nContent-Security-Policy: sandbox\n\n"));
    navigation->Commit();
    return navigation->GetFinalRenderFrameHost();
  }

  RenderFrameHost* AppendChildAndNavigate(RenderFrameHost* parent,
                                          const GURL& url) {
    RenderFrameHost* child =
        RenderFrameHostTester::For(parent)->AppendChild("child");
    RenderFrameHostTester::For(child)->InitializeRenderFrameIfNeeded();
    return NavigationSimulator::NavigateAndCommitFromDocument(url, child);
  }
};

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, MainFrame) {
  NavigateAndCommit(GURL("https://example.com/path?query#ref"));

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(main_rfh()));
}

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, SameOriginSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("https://example.com/child"));

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(child));
}

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, CrossOriginSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("https://other.com/child"));

  EXPECT_EQ(GURL("https://other.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(child));
}

// Opaque subframes never inherit their precursor origin.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, OpaqueSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("data:text/html,child"));
  ASSERT_TRUE(child->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(child));
}

// A sandboxed primary main frame is mapped to its precursor origin.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, SandboxedMainFrame) {
  RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// A sandboxed primary main frame keeps using its own precursor origin while a
// navigation to another origin is pending.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       SandboxedMainFrameWithPendingNavigation) {
  RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  const GURL pending_url("https://other.com/");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(pending_url, web_contents());
  navigation->Start();
  ASSERT_EQ(pending_url, web_contents()->GetVisibleURL());
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// The initial empty document has an opaque origin without a precursor.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, InitialEmptyDocument) {
  ASSERT_TRUE(main_rfh()->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(main_rfh()));
}

// Regression test for https://crbug.com/571195939: the visible URL of a pending
// navigation must not be used as the origin of the still-committed opaque
// initial empty document.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       InitialEmptyDocumentWithPendingNavigation) {
  RenderFrameHost* rfh = main_rfh();
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  const GURL pending_url("https://example.com/");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(pending_url, web_contents());
  navigation->Start();
  ASSERT_EQ(pending_url, web_contents()->GetVisibleURL());
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// Regression test for https://crbug.com/576129749. A sandboxed page navigates
// itself to about:blank. The about:blank document inherits the page's opaque
// origin (precursor https://example.com) and its sandbox flags. It is still a
// primary main frame, but its origin was inherited, not derived from its own
// URL, so it must not use the permissions of https://example.com.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       SandboxedMainFrameNavigatedToAboutBlank) {
  RenderFrameHost* sandboxed_rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(sandboxed_rfh->GetLastCommittedOrigin().opaque());

  RenderFrameHost* rfh = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(url::kAboutBlankURL), sandboxed_rfh);
  ASSERT_TRUE(rfh->IsInPrimaryMainFrame());
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());
  ASSERT_EQ(url::SchemeHostPort(GURL("https://example.com/")),
            rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// A same-document navigation keeps the document and its origin. A sandboxed
// page must keep using the permissions of its precursor afterwards.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       SandboxedMainFrameSameDocumentNavigation) {
  RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/path#fragment"), rfh);
  navigation->CommitSameDocument();
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// An opaque primary main frame whose URL matches its precursor origin, but to
// which no sandbox restrictions apply, must not use the precursor's
// permissions.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       UnsandboxedOpaqueMainFrameIsNotAttributedToPrecursor) {
  // A failed navigation commits such a document.
  auto navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/path"), web_contents());
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->CommitErrorPage();
  RenderFrameHost* rfh = navigation->GetFinalRenderFrameHost();
  ASSERT_TRUE(rfh->IsInPrimaryMainFrame());
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());
  ASSERT_EQ(url::SchemeHostPort(GURL("https://example.com/")),
            rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
  ASSERT_FALSE(rfh->IsSandboxed(network::mojom::WebSandboxFlags::kOrigin));

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

}  // namespace content
