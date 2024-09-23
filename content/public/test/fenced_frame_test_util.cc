// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fenced_frame_test_util.h"

#include <string_view>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/test/fenced_frame_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace content {
namespace test {
namespace {

constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.id = 'fencedframe'+$1;
    document.body.appendChild(fenced_frame);
  })";

constexpr char kAddAndNavigateFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.config = new FencedFrameConfig($1);
    document.body.appendChild(fenced_frame);
  })";

constexpr char kNavigateFrameScript[] = R"({location.href = $1;})";

constexpr char kEmbedderNavigateFencedFrameScript[] = R"({
  document.getElementById('fencedframe'+$1).config =
      new FencedFrameConfig($2);}
)";
}  // namespace

FencedFrameTestHelper::FencedFrameTestHelper() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{blink::features::kFencedFrames, {}},
       {features::kPrivacySandboxAdsAPIsOverride, {}},
       {blink::features::kInterestGroupStorage, {}},
       {blink::features::kAdInterestGroupAPI, {}},
       {blink::features::kFledge, {}},
       {blink::features::kFencedFramesAPIChanges, {}},
       {blink::features::kFencedFramesDefaultMode, {}},
       {features::kFencedFramesEnforceFocus, {}},
       {blink::features::kFencedFramesAutomaticBeaconCredentials, {}},
       {blink::features::kFencedFramesLocalUnpartitionedDataAccess, {}},
       {blink::features::kFencedFramesCrossOriginEventReportingUnlabeledTraffic,
        {}},
       {blink::features::kFencedFramesReportEventHeaderChanges, {}},
       {blink::features::kExemptUrlFromNetworkRevocationForTesting, {}}},
      {/* disabled_features */});
}

FencedFrameTestHelper::~FencedFrameTestHelper() = default;

RenderFrameHost* FencedFrameTestHelper::CreateFencedFrame(
    RenderFrameHost* fenced_frame_parent,
    const GURL& url,
    net::Error expected_error_code,
    blink::FencedFrame::DeprecatedFencedFrameMode mode,
    bool wait_for_load) {
  TRACE_EVENT("test", "FencedFrameTestHelper::CreateAndGetFencedFrame",
              "fenced_frame_parent", fenced_frame_parent, "url", url);
  RenderFrameHostImpl* fenced_frame_parent_rfh =
      static_cast<RenderFrameHostImpl*>(fenced_frame_parent);
  RenderFrameHostImpl* fenced_frame_rfh;
  size_t previous_fenced_frame_count =
      fenced_frame_parent_rfh->GetFencedFrames().size();

  EXPECT_TRUE(
      ExecJs(fenced_frame_parent_rfh,
             JsReplace(kAddFencedFrameScript,
                       base::NumberToString(previous_fenced_frame_count)),
             EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  std::vector<FencedFrame*> fenced_frames =
      fenced_frame_parent_rfh->GetFencedFrames();
  EXPECT_EQ(previous_fenced_frame_count + 1, fenced_frames.size());

  FencedFrame* fenced_frame = fenced_frames.back();
  // It is possible that we got the did stop loading notification because the
  // fenced frame was actually being destroyed. Check to make sure that's not
  // the case. TODO(crbug.com/40053214): Consider weakly referencing the fenced
  // frame if the removal-and-stop-loading scenario is a useful one to test.
  EXPECT_EQ(previous_fenced_frame_count + 1,
            fenced_frame_parent_rfh->GetFencedFrames().size());
  fenced_frame_rfh = fenced_frame->GetInnerRoot();
  if (url.is_empty())
    return fenced_frame_rfh;

  // For default mode, perform a content-initiated navigation (for backwards
  // compatibility with existing tests).
  if (mode == blink::FencedFrame::DeprecatedFencedFrameMode::kDefault) {
    return NavigateFrameInFencedFrameTree(fenced_frame_rfh, url,
                                          expected_error_code, wait_for_load);
  }

  // For opaque-ads mode, perform an embedder-initiated navigation, because only
  // embedder-initiation urn navigations make sense.
  EXPECT_EQ(mode, blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);
  GURL potentially_urn_url = url;
  std::optional<GURL> urn_uuid = fenced_frame_parent_rfh->GetPage()
                                     .fenced_frame_urls_map()
                                     .AddFencedFrameURLForTesting(url);
  EXPECT_TRUE(urn_uuid.has_value());
  EXPECT_TRUE(urn_uuid->is_valid());
  potentially_urn_url = *urn_uuid;

  FrameTreeNode* target_node = fenced_frame_rfh->frame_tree_node();
  TestFrameNavigationObserver fenced_frame_observer(fenced_frame_rfh);
  EXPECT_TRUE(
      ExecJs(fenced_frame_parent_rfh,
             JsReplace(kEmbedderNavigateFencedFrameScript,
                       base::NumberToString(previous_fenced_frame_count),
                       potentially_urn_url)));

  if (!wait_for_load) {
    return nullptr;
  }

  fenced_frame_observer.Wait();

  EXPECT_EQ(target_node->current_frame_host()->IsErrorDocument(),
            expected_error_code != net::OK);

  return target_node->current_frame_host();
}

void FencedFrameTestHelper::NavigateFencedFrameUsingFledge(
    RenderFrameHost* fenced_frame_parent,
    const GURL& url,
    const std::string fenced_frame_id) {
  // Run an ad auction using FLEDGE and load the result into the fenced frame
  // with id `fenced_frame_id`.
  EXPECT_TRUE(ExecJs(fenced_frame_parent, JsReplace(R"(
    (async() => {
      const FLEDGE_BIDDING_URL = "/interest_group/bidding_logic.js";
      const FLEDGE_DECISION_URL = "/interest_group/decision_logic.js";

      const page_origin = new URL($1).origin;
      const bidding_url = new URL(FLEDGE_BIDDING_URL, page_origin);
      const interest_group = {
        name: 'testAd1',
        owner: page_origin,
        biddingLogicUrl: bidding_url,
        ads: [{renderURL: $1, bid: 1, allowedReportingOrigins: [$1]}],
      };

      // Pick an arbitrarily high duration to guarantee that we never leave the
      // ad interest group while the test runs.
      await navigator.joinAdInterestGroup(
          interest_group, /*durationSeconds=*/3000000);

      const auction_config = {
        seller: page_origin,
        interestGroupBuyers: [page_origin],
        decisionLogicURL: new URL(FLEDGE_DECISION_URL, page_origin),
      };
      auction_config.resolveToConfig = true;

      const fenced_frame_config = await navigator.runAdAuction(auction_config);
      if (!(fenced_frame_config instanceof FencedFrameConfig)) {
        throw new Error('runAdAuction() did not return a FencedFrameConfig');
      }

      document.getElementById($2).config = fenced_frame_config;
    })())",
                                                    url, fenced_frame_id)));
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
    net::Error expected_error_code,
    bool wait_for_load) {
  TRACE_EVENT("test", "FencedFrameTestHelper::NavigateFrameInsideFencedFrame",
              "rfh", rfh, "url", url);
  // TODO(domfarolino): Consider adding |url| to the relevant
  // `FencedFrameURLMapping` and then actually passing in the urn:uuid to the
  // script below, so that we exercise the "real" navigation path.

  FrameTreeNode* target_node =
      static_cast<RenderFrameHostImpl*>(rfh)->frame_tree_node();

  TestFrameNavigationObserver fenced_frame_observer(rfh);
  EXPECT_EQ(url.spec(), EvalJs(rfh, JsReplace(kNavigateFrameScript, url)));

  if (!wait_for_load) {
    return nullptr;
  }

  fenced_frame_observer.Wait();

  EXPECT_EQ(target_node->current_frame_host()->IsErrorDocument(),
            expected_error_code != net::OK);

  return target_node->current_frame_host();
}

void FencedFrameTestHelper::SendBasicRequest(
    WebContents* web_contents,
    GURL url,
    std::optional<std::string> content) {
  // Construct the resource request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      web_contents->GetPrimaryMainFrame()
          ->GetStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  auto request = std::make_unique<network::ResourceRequest>();

  request->url = url;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->method = net::HttpRequestHeaders::kPostMethod;
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  if (content) {
    simple_url_loader->AttachStringForUpload(
        content.value(),
        /*upload_content_type=*/"text/plain;charset=UTF-8");
  }
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

  // Send out the reporting beacon.
  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::DoNothingWithBoundArgs(std::move(simple_url_loader)));
}

// static
RenderFrameHost* FencedFrameTestHelper::GetMostRecentlyAddedFencedFrame(
    RenderFrameHost* rfh) {
  std::vector<FencedFrame*> fenced_frames =
      static_cast<RenderFrameHostImpl*>(rfh)->GetFencedFrames();
  if (fenced_frames.empty())
    return nullptr;
  return fenced_frames.back()->GetInnerRoot();
}

// static
std::vector<RenderFrameHost*> FencedFrameTestHelper::GetChildFencedFrameHosts(
    RenderFrameHost* rfh) {
  std::vector<RenderFrameHost*> fenced_hosts;
  std::vector<FencedFrame*> fenced_frames =
      static_cast<RenderFrameHostImpl*>(rfh)->GetFencedFrames();
  for (FencedFrame* frame : fenced_frames) {
    fenced_hosts.push_back(frame->GetInnerRoot());
  }
  return fenced_hosts;
}

GURL CreateFencedFrameURLMapping(RenderFrameHost* rfh, const GURL& url) {
  FrameTreeNode* target_node =
      static_cast<RenderFrameHostImpl*>(rfh)->frame_tree_node();
  FencedFrameURLMapping& url_mapping =
      target_node->current_frame_host()->GetPage().fenced_frame_urls_map();
  return AddAndVerifyFencedFrameURL(&url_mapping, url);
}

GURL AddAndVerifyFencedFrameURL(
    FencedFrameURLMapping* fenced_frame_url_mapping,
    const GURL& https_url,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
  std::optional<GURL> urn_uuid =
      fenced_frame_url_mapping->AddFencedFrameURLForTesting(
          https_url, std::move(fenced_frame_reporter));
  EXPECT_TRUE(urn_uuid.has_value());
  EXPECT_TRUE(urn_uuid->is_valid());
  return urn_uuid.value();
}

void ExemptUrlsFromFencedFrameNetworkRevocation(RenderFrameHost* rfh,
                                                const std::vector<GURL>& urls) {
  base::ranges::for_each(urls, [rfh](GURL url) {
    static_cast<RenderFrameHostImpl*>(rfh)
        ->ExemptUrlFromNetworkRevocationForTesting(url, base::DoNothing());
  });
}

void SimulateClickInFencedFrameTree(const ToRenderFrameHost& adapter,
                                    blink::WebMouseEvent::Button button,
                                    const gfx::PointF& point) {
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = button;
  mouse_event.SetPositionInWidget(point);
  mouse_event.click_count = 1;
  adapter.render_frame_host()->GetRenderWidgetHost()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  adapter.render_frame_host()->GetRenderWidgetHost()->ForwardMouseEvent(
      mouse_event);
}

gfx::PointF GetTopLeftCoordinatesOfElementWithId(
    const ToRenderFrameHost& adapter,
    std::string_view id) {
  double x = EvalJs(adapter, content::JsReplace(R"(
                                  const bounds =
                                    document.getElementById($1).
                                    getBoundingClientRect();
                                  Math.floor(bounds.left)
                                )",
                                                id))
                 .ExtractDouble();
  double y = EvalJs(adapter, content::JsReplace(R"(
                                  const bounds =
                                    document.getElementById($1).
                                    getBoundingClientRect();
                                  Math.floor(bounds.top)
                                )",
                                                id))
                 .ExtractDouble();

  return gfx::PointF(x, y);
}

}  // namespace test

}  // namespace content
