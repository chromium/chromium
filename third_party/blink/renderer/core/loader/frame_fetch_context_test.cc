/*
 * Copyright (c) 2015, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"

namespace blink {

namespace {

class DummyFrameOwner final : public GarbageCollected<DummyFrameOwner>,
                              public FrameOwner {
 public:
  void Trace(Visitor* visitor) const override { FrameOwner::Trace(visitor); }

  // FrameOwner overrides:
  Frame* ContentFrame() const override { return nullptr; }
  void SetContentFrame(Frame&) override {}
  void ClearContentFrame() override {}
  const FramePolicy& GetFramePolicy() const override {
    DEFINE_STATIC_LOCAL(FramePolicy, frame_policy, ());
    return frame_policy;
  }
  void AddResourceTiming(mojom::blink::ResourceTimingInfoPtr) override {}
  void DispatchLoad() override {}
  void IntrinsicSizingInfoChanged() override {}
  void SetNeedsOcclusionTracking(bool) override {}
  AtomicString BrowsingContextContainerName() const override {
    return AtomicString();
  }
  mojom::blink::ScrollbarMode ScrollbarMode() const override {
    return mojom::blink::ScrollbarMode::kAuto;
  }
  int MarginWidth() const override { return -1; }
  int MarginHeight() const override { return -1; }
  bool AllowFullscreen() const override { return false; }
  bool AllowPaymentRequest() const override { return false; }
  bool IsDisplayNone() const override { return false; }
  mojom::blink::ColorScheme GetColorScheme() const override {
    return mojom::blink::ColorScheme::kLight;
  }
  mojom::blink::PreferredColorScheme GetPreferredColorScheme() const override {
    return mojom::blink::PreferredColorScheme::kLight;
  }
  bool ShouldLazyLoadChildren() const override { return false; }

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already DummyFrameOwner.
  bool IsLocal() const override { return false; }
  bool IsRemote() const override { return false; }
};

}  // namespace

using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;

class FrameFetchContextMockLocalFrameClient : public EmptyLocalFrameClient {
 public:
  FrameFetchContextMockLocalFrameClient() : EmptyLocalFrameClient() {}
  MOCK_METHOD0(DidDisplayContentWithCertificateErrors, void());
  MOCK_METHOD2(DispatchDidLoadResourceFromMemoryCache,
               void(const ResourceRequest&, const ResourceResponse&));
  MOCK_METHOD0(UserAgent, String());
  MOCK_METHOD0(MayUseClientLoFiForImageRequests, bool());
};

class FixedPolicySubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  FixedPolicySubresourceFilter(LoadPolicy policy,
                               int* filtered_load_counter,
                               bool is_associated_with_ad_subframe)
      : policy_(policy), filtered_load_counter_(filtered_load_counter) {}

  LoadPolicy GetLoadPolicy(const WebURL& resource_url,
                           mojom::blink::RequestContextType) override {
    return policy_;
  }

  LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL& url) override {
    return policy_;
  }

  LoadPolicy GetLoadPolicyForWebTransportConnect(const WebURL&) override {
    return policy_;
  }
  void ReportDisallowedLoad() override { ++*filtered_load_counter_; }

  bool ShouldLogToConsole() override { return false; }

 private:
  const LoadPolicy policy_;
  int* filtered_load_counter_;
};

class FrameFetchContextTest : public testing::Test {
 protected:
  void SetUp() override { RecreateFetchContext(); }

  void RecreateFetchContext(
      const KURL& url = KURL(),
      const String& permissions_policy_header = String()) {
    dummy_page_holder = nullptr;
    dummy_page_holder = std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
    if (url.IsValid()) {
      auto params = WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
      if (!permissions_policy_header.empty()) {
        params->response.SetHttpHeaderField(http_names::kFeaturePolicy,
                                            permissions_policy_header);
      }
      dummy_page_holder->GetFrame().Loader().CommitNavigation(
          std::move(params), nullptr /* extra_data */);
      blink::test::RunPendingTasks();
      ASSERT_EQ(url.GetString(),
                dummy_page_holder->GetDocument().Url().GetString());
    }
    document = &dummy_page_holder->GetDocument();
    owner = MakeGarbageCollected<DummyFrameOwner>();
  }

  FrameFetchContext* GetFetchContext() {
    return static_cast<FrameFetchContext*>(&document->Fetcher()->Context());
  }

  // Call the method for the actual test cases as only this fixture is specified
  // as a friend class.
  void SetFirstPartyCookie(ResourceRequest& request) {
    GetFetchContext()->SetFirstPartyCookie(request);
  }

  scoped_refptr<const SecurityOrigin> GetTopFrameOrigin() {
    return GetFetchContext()->GetTopFrameOrigin();
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder;
  // We don't use the DocumentLoader directly in any tests, but need to keep it
  // around as long as the ResourceFetcher and Document live due to indirect
  // usage.
  Persistent<Document> document;

  Persistent<DummyFrameOwner> owner;
};

class FrameFetchContextSubresourceFilterTest : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    FrameFetchContextTest::SetUp();
    filtered_load_callback_counter_ = 0;
  }

  int GetFilteredLoadCallCount() const {
    return filtered_load_callback_counter_;
  }

  void SetFilterPolicy(WebDocumentSubresourceFilter::LoadPolicy policy,
                       bool is_associated_with_ad_subframe = false) {
    document->Loader()->SetSubresourceFilter(new FixedPolicySubresourceFilter(
        policy, &filtered_load_callback_counter_,
        is_associated_with_ad_subframe));
  }

  std::optional<ResourceRequestBlockedReason> CanRequest() {
    return CanRequestInternal(ReportingDisposition::kReport);
  }

  std::optional<ResourceRequestBlockedReason> CanRequestKeepAlive() {
    return CanRequestInternal(ReportingDisposition::kReport,
                              true /* keepalive */);
  }

  std::optional<ResourceRequestBlockedReason> CanRequestPreload() {
    return CanRequestInternal(ReportingDisposition::kSuppressReporting);
  }

  std::optional<ResourceRequestBlockedReason> CanRequestAndVerifyIsAd(
      bool expect_is_ad) {
    std::optional<ResourceRequestBlockedReason> reason =
        CanRequestInternal(ReportingDisposition::kReport);
    ResourceRequest request(KURL("http://example.com/"));
    FetchInitiatorInfo initiator_info;
    EXPECT_EQ(expect_is_ad, GetFetchContext()->CalculateIfAdSubresource(
                                request, std::nullopt /* alias_url */,
                                ResourceType::kMock, initiator_info));
    return reason;
  }

 private:
  std::optional<ResourceRequestBlockedReason> CanRequestInternal(
      ReportingDisposition reporting_disposition,
      bool keepalive = false) {
    const KURL input_url("http://example.com/");
    ResourceRequest resource_request(input_url);
    resource_request.SetKeepalive(keepalive);
    resource_request.SetRequestorOrigin(document->Fetcher()
                                            ->GetProperties()
                                            .GetFetchClientSettingsObject()
                                            .GetSecurityOrigin());
    ResourceLoaderOptions options(nullptr /* world */);
    // DJKim
    return GetFetchContext()->CanRequest(ResourceType::kImage, resource_request,
                                         input_url, options,
                                         reporting_disposition, std::nullopt);
  }

  int filtered_load_callback_counter_;
};

// This test class sets up a mock frame loader client.
class FrameFetchContextMockedLocalFrameClientTest
    : public FrameFetchContextTest {
 protected:
  void SetUp() override {
    url = KURL("https://example.test/foo");
    http_url = KURL("http://example.test/foo");
    main_resource_url = KURL("https://example.test");
    different_host_url = KURL("https://different.example.test/foo");
    client = MakeGarbageCollected<
        testing::NiceMock<FrameFetchContextMockLocalFrameClient>>();
    dummy_page_holder =
        std::make_unique<DummyPageHolder>(gfx::Size(500, 500), nullptr, client);
    Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
    document = &dummy_page_holder->GetDocument();
    document->SetURL(main_resource_url);
    owner = MakeGarbageCollected<DummyFrameOwner>();
  }

  KURL url;
  KURL http_url;
  KURL main_resource_url;
  KURL different_host_url;

  Persistent<testing::NiceMock<FrameFetchContextMockLocalFrameClient>> client;
};

class FrameFetchContextModifyRequestTest : public FrameFetchContextTest {
 public:
  FrameFetchContextModifyRequestTest()
      : example_origin(SecurityOrigin::Create(KURL("https://example.test/"))) {}

 protected:
  void ModifyRequestForCSP(ResourceRequest& resource_request,
                           mojom::RequestContextFrameType frame_type) {
    document->GetFrame()->Loader().ModifyRequestForCSP(
        resource_request,
        &document->Fetcher()->GetProperties().GetFetchClientSettingsObject(),
        document->domWindow(), frame_type);
  }

  void ExpectUpgrade(const char* input, const char* expected) {
    ExpectUpgrade(input, mojom::blink::RequestContextType::SCRIPT,
                  mojom::RequestContextFrameType::kNone, expected);
  }

  void ExpectUpgrade(const char* input,
                     mojom::blink::RequestContextType request_context,
                     mojom::RequestContextFrameType frame_type,
                     const char* expected) {
    const KURL input_url(input);
    const KURL expected_url(expected);

    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(request_context);

    ModifyRequestForCSP(resource_request, frame_type);

    EXPECT_EQ(expected_url.GetString(), resource_request.Url().GetString());
    EXPECT_EQ(expected_url.Protocol(), resource_request.Url().Protocol());
    EXPECT_EQ(expected_url.Host(), resource_request.Url().Host());
    EXPECT_EQ(expected_url.Port(), resource_request.Url().Port());
    EXPECT_EQ(expected_url.HasPort(), resource_request.Url().HasPort());
    EXPECT_EQ(expected_url.GetPath(), resource_request.Url().GetPath());
  }

  void ExpectUpgradeInsecureRequestHeader(
      const char* input,
      mojom::RequestContextFrameType frame_type,
      bool should_prefer) {
    const KURL input_url(input);

    ResourceRequest resource_request(input_url);
    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::SCRIPT);

    ModifyRequestForCSP(resource_request, frame_type);

    EXPECT_EQ(
        should_prefer ? String("1") : String(),
        resource_request.HttpHeaderField(http_names::kUpgradeInsecureRequests));

    // Calling modifyRequestForCSP more than once shouldn't affect the
    // header.
    if (should_prefer) {
      GetFetchContext()->ModifyRequestForCSP(resource_request);
      EXPECT_EQ("1", resource_request.HttpHeaderField(
                         http_names::kUpgradeInsecureRequests));
    }
  }

  void ExpectIsAutomaticUpgradeSet(const char* input,
                                   const char* main_frame,
                                   mojom::blink::InsecureRequestPolicy policy,
                                   bool expected_value) {
    const KURL input_url(input);
    const KURL main_frame_url(main_frame);
    ResourceRequest resource_request(input_url);
    // TODO(crbug.com/1026464, carlosil): Default behavior currently is to not
    // autoupgrade images, setting the context to AUDIO to ensure the upgrade
    // flow runs, this can be switched back to IMAGE once autoupgrades launch
    // for them.
    resource_request.SetRequestContext(mojom::blink::RequestContextType::AUDIO);

    RecreateFetchContext(main_frame_url);
    document->domWindow()->GetSecurityContext().SetInsecureRequestPolicy(
        policy);

    ModifyRequestForCSP(resource_request,
                        mojom::RequestContextFrameType::kNone);

    EXPECT_EQ(expected_value, resource_request.IsAutomaticUpgrade());
  }

  void SetFrameOwnerBasedOnFrameType(mojom::RequestContextFrameType frame_type,
                                     HTMLIFrameElement* iframe,
                                     const AtomicString& potential_value) {
    if (frame_type != mojom::RequestContextFrameType::kNested) {
      document->GetFrame()->SetOwner(nullptr);
      return;
    }

    iframe->setAttribute(html_names::kCspAttr, potential_value);
    document->GetFrame()->SetOwner(iframe);
  }

  scoped_refptr<const SecurityOrigin> example_origin;
};

TEST_F(FrameFetchContextModifyRequestTest, UpgradeInsecureResourceRequests) {
  struct TestCase {
    const char* original;
    const char* upgraded;
  } tests[] = {
      {"http://example.test/image.png", "https://example.test/image.png"},
      {"http://example.test:80/image.png",
       "https://example.test:443/image.png"},
      {"http://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"https://example.test/image.png", "https://example.test/image.png"},
      {"https://example.test:80/image.png",
       "https://example.test:80/image.png"},
      {"https://example.test:1212/image.png",
       "https://example.test:1212/image.png"},

      {"ftp://example.test/image.png", "ftp://example.test/image.png"},
      {"ftp://example.test:21/image.png", "ftp://example.test:21/image.png"},
      {"ftp://example.test:1212/image.png",
       "ftp://example.test:1212/image.png"},
  };

  document->domWindow()->GetSecurityContext().SetInsecureRequestPolicy(
      mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests);

  for (const auto& test : tests) {
    document->domWindow()
        ->GetSecurityContext()
        .ClearInsecureNavigationsToUpgradeForTest();

    // We always upgrade for FrameTypeNone.
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::SCRIPT,
                  mojom::RequestContextFrameType::kNone, test.upgraded);

    // We never upgrade for FrameTypeNested. This is done on the browser
    // process.
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::SCRIPT,
                  mojom::RequestContextFrameType::kNested, test.original);

    // We do not upgrade for FrameTypeTopLevel or FrameTypeAuxiliary...
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::SCRIPT,
                  mojom::RequestContextFrameType::kTopLevel, test.original);
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::SCRIPT,
                  mojom::RequestContextFrameType::kAuxiliary, test.original);

    // unless the request context is RequestContextForm.
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::FORM,
                  mojom::RequestContextFrameType::kTopLevel, test.upgraded);
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::FORM,
                  mojom::RequestContextFrameType::kAuxiliary, test.upgraded);

    // Or unless the host of the resource is in the document's
    // InsecureNavigationsSet:
    document->domWindow()->GetSecurityContext().AddInsecureNavigationUpgrade(
        example_origin->Host().Impl()->GetHash());
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::SCRIPT,
                  mojom::RequestContextFrameType::kTopLevel, test.upgraded);
    ExpectUpgrade(test.original, mojom::blink::RequestContextType::SCRIPT,
                  mojom::RequestContextFrameType::kAuxiliary, test.upgraded);
  }
}

TEST_F(FrameFetchContextModifyRequestTest,
       DoNotUpgradeInsecureResourceRequests) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kMixedContentAutoupgrade);

  RecreateFetchContext(KURL("https://secureorigin.test/image.png"));
  document->domWindow()->GetSecurityContext().SetInsecureRequestPolicy(
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone);

  ExpectUpgrade("http://example.test/image.png",
                "http://example.test/image.png");
  ExpectUpgrade("http://example.test:80/image.png",
                "http://example.test:80/image.png");
  ExpectUpgrade("http://example.test:1212/image.png",
                "http://example.test:1212/image.png");

  ExpectUpgrade("https://example.test/image.png",
                "https://example.test/image.png");
  ExpectUpgrade("https://example.test:80/image.png",
                "https://example.test:80/image.png");
  ExpectUpgrade("https://example.test:1212/image.png",
                "https://example.test:1212/image.png");

  ExpectUpgrade("ftp://example.test/image.png", "ftp://example.test/image.png");
  ExpectUpgrade("ftp://example.test:21/image.png",
                "ftp://example.test:21/image.png");
  ExpectUpgrade("ftp://example.test:1212/image.png",
                "ftp://example.test:1212/image.png");
}

TEST_F(FrameFetchContextModifyRequestTest, IsAutomaticUpgradeSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kMixedContentAutoupgrade);
  ExpectIsAutomaticUpgradeSet(
      "http://example.test/image.png", "https://example.test",
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone, true);
}

TEST_F(FrameFetchContextModifyRequestTest, IsAutomaticUpgradeNotSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kMixedContentAutoupgrade);
  // Upgrade shouldn't happen if the resource is already https.
  ExpectIsAutomaticUpgradeSet(
      "https://example.test/image.png", "https://example.test",
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone, false);
  // Upgrade shouldn't happen if the site is http.
  ExpectIsAutomaticUpgradeSet(
      "http://example.test/image.png", "http://example.test",
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone, false);

  // Flag shouldn't be set if upgrade was due to upgrade-insecure-requests.
  ExpectIsAutomaticUpgradeSet(
      "http://example.test/image.png", "https://example.test",
      mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests, false);
}

TEST_F(FrameFetchContextModifyRequestTest, SendUpgradeInsecureRequestHeader) {
  struct TestCase {
    const char* to_request;
    mojom::RequestContextFrameType frame_type;
    bool should_prefer;
  } tests[] = {{"http://example.test/page.html",
                mojom::RequestContextFrameType::kAuxiliary, true},
               {"http://example.test/page.html",
                mojom::RequestContextFrameType::kNested, true},
               {"http://example.test/page.html",
                mojom::RequestContextFrameType::kNone, false},
               {"http://example.test/page.html",
                mojom::RequestContextFrameType::kTopLevel, true},
               {"https://example.test/page.html",
                mojom::RequestContextFrameType::kAuxiliary, true},
               {"https://example.test/page.html",
                mojom::RequestContextFrameType::kNested, true},
               {"https://example.test/page.html",
                mojom::RequestContextFrameType::kNone, false},
               {"https://example.test/page.html",
                mojom::RequestContextFrameType::kTopLevel, true}};

  // This should work correctly both when the FrameFetchContext has a Document,
  // and when it doesn't (e.g. during main frame navigations), so run through
  // the tests both before and after providing a document to the context.
  for (const auto& test : tests) {
    document->domWindow()->GetSecurityContext().SetInsecureRequestPolicy(
        mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);

    document->domWindow()->GetSecurityContext().SetInsecureRequestPolicy(
        mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);
  }

  for (const auto& test : tests) {
    document->domWindow()->GetSecurityContext().SetInsecureRequestPolicy(
        mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);

    document->domWindow()->GetSecurityContext().SetInsecureRequestPolicy(
        mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests);
    ExpectUpgradeInsecureRequestHeader(test.to_request, test.frame_type,
                                       test.should_prefer);
  }
}

class FrameFetchContextHintsTest : public FrameFetchContextTest,
                                   public testing::WithParamInterface<bool> {
 public:
  FrameFetchContextHintsTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
    };
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (GetParam()) {
      enabled_features.push_back(
          blink::features::kQuoteEmptySecChUaStringHeadersConsistently);
    } else {
      disabled_features.push_back(
          blink::features::kQuoteEmptySecChUaStringHeadersConsistently);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    // Set the document URL to a secure document.
    RecreateFetchContext(KURL("https://www.example.com/"));
    Settings* settings = document->GetSettings();
    settings->SetScriptEnabled(true);
  }

 protected:
  void ExpectHeader(const char* input,
                    const char* header_name,
                    bool is_present,
                    const char* header_value,
                    float width = 0) {
    SCOPED_TRACE(testing::Message() << header_name);

    std::optional<float> resource_width;
    if (width > 0) {
      resource_width = width;
    }

    const KURL input_url(input);
    ResourceRequest resource_request(input_url);

    GetFetchContext()->AddClientHintsIfNecessary(resource_width,
                                                 resource_request);

    String expected = is_present ? String(header_value) : String();
    EXPECT_EQ(expected,
              resource_request.HttpHeaderField(AtomicString(header_name)));
  }

  // Returns the expected value for a header containing an empty string. This
  // should be `""`, but if !kQuoteEmptySecChUaStringHeadersConsistently then
  // it is instead an empty string.
  const char* EmptyString() {
    if (base::FeatureList::IsEnabled(
            blink::features::kQuoteEmptySecChUaStringHeadersConsistently)) {
      return "\"\"";
    } else {
      return "";
    }
  }

  String GetHeaderValue(const char* input, const char* header_name) {
    const KURL input_url(input);
    ResourceRequest resource_request(input_url);
    GetFetchContext()->AddClientHintsIfNecessary(
        std::nullopt /* resource_width */, resource_request);
    return resource_request.HttpHeaderField(AtomicString(header_name));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FrameFetchContextHintsTest,
                         testing::ValuesIn({false, true}));
// Verify that the client hints should be attached for subresources fetched
// over secure transport. Tests when the persistent client hint feature is
// enabled.
TEST_P(FrameFetchContextHintsTest, MonitorDeviceMemorySecureTransport) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Device-Memory", true,
               "4");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Viewport-Width", false,
               "");
  ExpectHeader("https://www.someother-example.com/1.gif", "Device-Memory",
               false, "");
  ExpectHeader("https://www.someother-example.com/1.gif",
               "Sec-CH-Device-Memory", false, "");
}

// Verify that client hints are not attached when the resources do not belong to
// a secure context.
TEST_P(FrameFetchContextHintsTest, MonitorDeviceMemoryHintsInsecureContext) {
  // Verify that client hints are not attached when the resources do not belong
  // to a secure context and the persistent client hint features is enabled.
  ExpectHeader("http://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("http://www.example.com/1.gif", "Device-Memory", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Device-Memory", false,
               "");
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-DPR", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Viewport-Width", false,
               "");
}

// Verify that client hints are attched when the resources belong to a local
// context.
TEST_P(FrameFetchContextHintsTest, MonitorDeviceMemoryHintsLocalContext) {
  RecreateFetchContext(KURL("http://localhost/"));
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("http://localhost/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("http://localhost/1.gif", "Device-Memory", true, "4");
  ExpectHeader("http://localhost/1.gif", "Sec-CH-Device-Memory", true, "4");
  ExpectHeader("http://localhost/1.gif", "DPR", false, "");
  ExpectHeader("http://localhost/1.gif", "Sec-CH-DPR", false, "");
  ExpectHeader("http://localhost/1.gif", "Width", false, "");
  ExpectHeader("http://localhost/1.gif", "Sec-CH-Width", false, "");
  ExpectHeader("http://localhost/1.gif", "Viewport-Width", false, "");
  ExpectHeader("http://localhost/1.gif", "Sec-CH-Viewport-Width", false, "");
}

TEST_P(FrameFetchContextHintsTest, MonitorDeviceMemoryHints) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Device-Memory", true,
               "4");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(2048);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "2");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Device-Memory", true,
               "2");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(64385);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "8");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Device-Memory", true,
               "8");
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(768);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "0.5");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Device-Memory", true,
               "0.5");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Viewport-Width", false,
               "");
}

TEST_P(FrameFetchContextHintsTest, MonitorDPRHints) {
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDpr);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "1");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-DPR", true, "1");
  document->GetFrame()->SetLayoutZoomFactor(2.5);
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "2.5");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-DPR", true, "2.5");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Viewport-Width", false,
               "");
}

TEST_P(FrameFetchContextHintsTest, MonitorDPRHintsInsecureTransport) {
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-DPR", false, "");
  document->GetFrame()->SetLayoutZoomFactor(2.5);
  ExpectHeader("http://www.example.com/1.gif", "DPR", false, "  ");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-DPR", false, "  ");
  ExpectHeader("http://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Viewport-Width", false,
               "");
}

TEST_P(FrameFetchContextHintsTest, MonitorResourceWidthHints) {
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kResourceWidth);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "500", 500);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", true, "500",
               500);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "667", 666.6666);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", true, "667",
               666.6666);
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-DPR", false, "");

  document->GetFrame()->SetLayoutZoomFactor(2.5);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "1250", 500);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", true, "1250",
               500);
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "1667",
               666.6666);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", true, "1667",
               666.6666);
}

TEST_P(FrameFetchContextHintsTest, MonitorViewportWidthHints) {
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kViewportWidth);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "500");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Viewport-Width", true,
               "500");
  dummy_page_holder->GetFrameView().SetLayoutSizeFixedToFrameSize(false);
  dummy_page_holder->GetFrameView().SetLayoutSize(gfx::Size(800, 800));
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "800");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Viewport-Width", true,
               "800");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "800",
               666.6666);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Viewport-Width", true,
               "800", 666.6666);
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-DPR", false, "");
}

TEST_P(FrameFetchContextHintsTest, MonitorUAHints) {
  // `Sec-CH-UA` is always sent for secure requests
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA", true, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA", false, "");

  // `Sec-CH-UA-*` requires opt-in.
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
               false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
               false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors", false,
               "");
  ExpectHeader("http://www.example.com/0.gif", "Sec-CH-UA-Model", false, "");
  ExpectHeader("http://www.example.com/0.gif", "Sec-CH-UA-Form-Factors", false,
               "");

  {
    ClientHintsPreferences preferences;
    preferences.SetShouldSend(network::mojom::WebClientHintsType::kUAArch);
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", true,
                 EmptyString());
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");
  }

  {
    ClientHintsPreferences preferences;
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", true,
                 EmptyString());
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");
  }

  {
    ClientHintsPreferences preferences;
    preferences.SetShouldSend(
        network::mojom::WebClientHintsType::kUAPlatformVersion);
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 true, EmptyString());
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");
  }

  {
    ClientHintsPreferences preferences;
    preferences.SetShouldSend(network::mojom::WebClientHintsType::kUAModel);
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", true,
                 EmptyString());
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");
  }

  {
    ClientHintsPreferences preferences;
    preferences.SetShouldSend(
        network::mojom::WebClientHintsType::kUAFormFactors);
    document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 true, "");

    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
                 false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
    ExpectHeader("http://www.example.com/1.gif", "Sec-CH-UA-Form-Factors",
                 false, "");
  }
}

TEST_P(FrameFetchContextHintsTest, MonitorPrefersColorSchemeHint) {
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               false, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersColorScheme);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               true, "light");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               false, "");

  document->GetSettings()->SetPreferredColorScheme(
      mojom::PreferredColorScheme::kDark);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               true, "dark");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               false, "");
}

TEST_P(FrameFetchContextHintsTest, MonitorPrefersReducedMotionHint) {
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               false, "");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               false, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedMotion);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               true, "no-preference");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               false, "");

  document->GetSettings()->SetPrefersReducedMotion(true);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               true, "reduce");
  ExpectHeader("http://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               false, "");
}

TEST_P(FrameFetchContextHintsTest, MonitorPrefersReducedTransparencyHint) {
  ExpectHeader("https://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", false, "");
  ExpectHeader("http://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", false, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedTransparency);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

  ExpectHeader("https://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", true, "no-preference");
  ExpectHeader("http://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", false, "");

  document->GetSettings()->SetPrefersReducedTransparency(true);
  ExpectHeader("https://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", true, "reduce");
  ExpectHeader("http://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", false, "");
}

TEST_P(FrameFetchContextHintsTest, MonitorAllHints) {
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ExpectHeader("https://www.example.com/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.com/1.gif", "rtt", false, "");
  ExpectHeader("https://www.example.com/1.gif", "downlink", false, "");
  ExpectHeader("https://www.example.com/1.gif", "ect", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
               false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors", false,
               "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               false, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               false, "");
  ExpectHeader("https://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", false, "");

  // `Sec-CH-UA` is special.
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA", true, "");

  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDpr);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kResourceWidth);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kViewportWidth);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kRtt_DEPRECATED);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDownlink_DEPRECATED);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kEct_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kUA);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kUAArch);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kUAPlatformVersion);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kUAModel);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kUAFormFactors);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersColorScheme);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedMotion);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedTransparency);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Device-Memory", true,
               "4");
  ExpectHeader("https://www.example.com/1.gif", "DPR", true, "1");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-DPR", true, "1");
  ExpectHeader("https://www.example.com/1.gif", "Width", true, "400", 400);
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Width", true, "400",
               400);
  ExpectHeader("https://www.example.com/1.gif", "Viewport-Width", true, "500");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Viewport-Width", true,
               "500");

  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA", true, "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Arch", true,
               EmptyString());
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform", true,
               EmptyString());
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Platform-Version",
               true, EmptyString());
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Model", true,
               EmptyString());
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-UA-Form-Factors", true,
               "");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Color-Scheme",
               true, "light");
  ExpectHeader("https://www.example.com/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               true, "no-preference");
  ExpectHeader("https://www.example.com/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", true, "no-preference");

  // Value of network quality client hints may vary, so only check if the
  // header is present and the values are non-negative/non-empty.
  bool conversion_ok = false;
  int rtt_header_value = GetHeaderValue("https://www.example.com/1.gif", "rtt")
                             .ToIntStrict(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, rtt_header_value);

  float downlink_header_value =
      GetHeaderValue("https://www.example.com/1.gif", "downlink")
          .ToFloat(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, downlink_header_value);

  EXPECT_LT(
      0u,
      GetHeaderValue("https://www.example.com/1.gif", "ect").Ascii().length());
}

// Verify that the client hints should be attached for third-party subresources
// fetched over secure transport, when specifically allowed by permissions
// policy.
TEST_P(FrameFetchContextHintsTest, MonitorAllHintsPermissionsPolicy) {
  RecreateFetchContext(
      KURL("https://www.example.com/"),
      "ch-dpr *; ch-device-memory *; ch-downlink *; ch-ect *; ch-rtt *; ch-ua "
      "*; ch-ua-arch *; ch-ua-platform *; ch-ua-platform-version *; "
      "ch-ua-model *; ch-viewport-width *; ch-width *; ch-prefers-color-scheme "
      "*; ch-prefers-reduced-motion *; ch-prefers-reduced-transparency *");
  document->GetSettings()->SetScriptEnabled(true);
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDpr);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kResourceWidth);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kViewportWidth);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kRtt_DEPRECATED);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDownlink_DEPRECATED);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kEct_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kUA);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kUAArch);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kUAPlatformVersion);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kUAModel);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersColorScheme);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedMotion);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kPrefersReducedTransparency);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);

  // Verify that all client hints are sent to a third-party origin, with this
  // permissions policy header.
  ExpectHeader("https://www.example.net/1.gif", "DPR", true, "1");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-DPR", true, "1");
  ExpectHeader("https://www.example.net/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Device-Memory", true,
               "4");

  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA", true, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Arch", true,
               EmptyString());
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Platform", true,
               EmptyString());
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Platform-Version",
               true, EmptyString());
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Model", true,
               EmptyString());
  ExpectHeader("https://www.example.net/1.gif", "Width", true, "400", 400);
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Width", true, "400",
               400);
  ExpectHeader("https://www.example.net/1.gif", "Viewport-Width", true, "500");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Viewport-Width", true,
               "500");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Prefers-Color-Scheme",
               true, "light");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               true, "no-preference");
  ExpectHeader("https://www.example.net/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", true, "no-preference");

  // Value of network quality client hints may vary, so only check if the
  // header is present and the values are non-negative/non-empty.
  bool conversion_ok = false;
  int rtt_header_value = GetHeaderValue("https://www.example.com/1.gif", "rtt")
                             .ToIntStrict(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, rtt_header_value);

  float downlink_header_value =
      GetHeaderValue("https://www.example.com/1.gif", "downlink")
          .ToFloat(&conversion_ok);
  EXPECT_TRUE(conversion_ok);
  EXPECT_LE(0, downlink_header_value);

  EXPECT_LT(
      0u,
      GetHeaderValue("https://www.example.com/1.gif", "ect").Ascii().length());
}

// Verify that only the specifically allowed client hints are attached for
// third-party subresources fetched over secure transport.
TEST_P(FrameFetchContextHintsTest, MonitorSomeHintsPermissionsPolicy) {
  RecreateFetchContext(KURL("https://www.example.com/"),
                       "ch-device-memory 'self' https://www.example.net");
  document->GetSettings()->SetScriptEnabled(true);
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDpr);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  // With a permissions policy header, the client hints should be sent to the
  // declared third party origins.
  ExpectHeader("https://www.example.net/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Device-Memory", true,
               "4");
  ExpectHeader("https://www.someother-example.com/1.gif", "Device-Memory",
               false, "");
  ExpectHeader("https://www.someother-example.com/1.gif",
               "Sec-CH-Device-Memory", false, "");
  // `Sec-CH-UA` is special.
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA", true, "");

  // Other hints not declared in the policy are still not attached.
  ExpectHeader("https://www.example.net/1.gif", "downlink", false, "");
  ExpectHeader("https://www.example.net/1.gif", "ect", false, "");
  ExpectHeader("https://www.example.net/1.gif", "DPR", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-DPR", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Arch", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Platform-Version",
               false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-UA-Model", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Width", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Width", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Viewport-Width", false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Viewport-Width", false,
               "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Prefers-Color-Scheme",
               false, "");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Prefers-Reduced-Motion",
               false, "");
  ExpectHeader("https://www.example.net/1.gif",
               "Sec-CH-Prefers-Reduced-Transparency", false, "");
}

// Verify that the client hints are not attached for third-party subresources
// fetched over insecure transport, even when specifically allowed by
// permissions policy.
TEST_P(FrameFetchContextHintsTest,
       MonitorHintsPermissionsPolicyInsecureContext) {
  RecreateFetchContext(KURL("https://www.example.com/"), "ch-device-memory *");
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("https://www.example.com/1.gif", "Device-Memory", false, "");
  ClientHintsPreferences preferences;
  preferences.SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  preferences.SetShouldSend(network::mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().UpdateFrom(preferences);
  ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(4096);
  // Device-Memory hint in this case is sent to all (and only) secure origins.
  ExpectHeader("https://www.example.net/1.gif", "Device-Memory", true, "4");
  ExpectHeader("https://www.example.net/1.gif", "Sec-CH-Device-Memory", true,
               "4");
  ExpectHeader("http://www.example.net/1.gif", "Device-Memory", false, "");
  ExpectHeader("http://www.example.net/1.gif", "Sec-CH-Device-Memory", false,
               "");
}

TEST_F(FrameFetchContextTest, SubResourceCachePolicy) {
  // Reset load event state: if the load event is finished, we ignore the
  // DocumentLoader load type.
  document->open();
  ASSERT_FALSE(document->LoadEventFinished());

  // Default case
  ResourceRequest request("http://www.example.com/mock");
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // WebFrameLoadType::kReload should not affect sub-resources
  document->Loader()->SetLoadType(WebFrameLoadType::kReload);
  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // Conditional request
  document->Loader()->SetLoadType(WebFrameLoadType::kStandard);
  ResourceRequest conditional("http://www.example.com/mock");
  conditional.SetHttpHeaderField(http_names::kIfModifiedSince,
                                 AtomicString("foo"));
  EXPECT_EQ(mojom::FetchCacheMode::kValidateCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                conditional, ResourceType::kMock, FetchParameters::kNoDefer));

  // WebFrameLoadType::kReloadBypassingCache
  document->Loader()->SetLoadType(WebFrameLoadType::kReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // WebFrameLoadType::kReloadBypassingCache with a conditional request
  document->Loader()->SetLoadType(WebFrameLoadType::kReloadBypassingCache);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                conditional, ResourceType::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation
  document->Loader()->SetLoadType(WebFrameLoadType::kBackForward);
  EXPECT_EQ(mojom::FetchCacheMode::kForceCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kMock, FetchParameters::kNoDefer));

  // Back/forward navigation with a conditional request
  document->Loader()->SetLoadType(WebFrameLoadType::kBackForward);
  EXPECT_EQ(mojom::FetchCacheMode::kForceCache,
            GetFetchContext()->ResourceRequestCachePolicy(
                conditional, ResourceType::kMock, FetchParameters::kNoDefer));
}

// Tests if "Save-Data" header is correctly added on the first load and reload.
TEST_P(FrameFetchContextHintsTest, EnableDataSaver) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext(KURL("https://www.example.com/"));
  document->GetSettings()->SetScriptEnabled(true);

  ExpectHeader("https://www.example.com/", "Save-Data", true, "on");

  // Subsequent call to addAdditionalRequestHeaders should not append to the
  // save-data header.
  ExpectHeader("https://www.example.com/", "Save-Data", true, "on");
}

// Tests if "Save-Data" header is not added when the data saver is disabled.
TEST_P(FrameFetchContextHintsTest, DisabledDataSaver) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext(KURL("https://www.example.com/"));
  document->GetSettings()->SetScriptEnabled(true);

  ExpectHeader("https://www.example.com/", "Save-Data", false, "");
}

// Tests if reload variants can reflect the current data saver setting.
TEST_P(FrameFetchContextHintsTest, ChangeDataSaverConfig) {
  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  // Recreate the fetch context so that the updated save data settings are read.
  RecreateFetchContext(KURL("https://www.example.com/"));
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("https://www.example.com/", "Save-Data", true, "on");

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  RecreateFetchContext(KURL("https://www.example.com/"));
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("https://www.example.com/", "Save-Data", false, "");

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);
  RecreateFetchContext(KURL("https://www.example.com/"));
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("https://www.example.com/", "Save-Data", true, "on");

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(false);
  RecreateFetchContext(KURL("https://www.example.com/"));
  document->GetSettings()->SetScriptEnabled(true);
  ExpectHeader("https://www.example.com/", "Save-Data", false, "");
}

TEST_F(FrameFetchContextSubresourceFilterTest, Filter) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kDisallow);

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(1, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(2, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestPreload());
  EXPECT_EQ(2, GetFilteredLoadCallCount());

  EXPECT_EQ(ResourceRequestBlockedReason::kSubresourceFilter,
            CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(3, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, Allow) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kAllow);

  EXPECT_EQ(std::nullopt, CanRequestAndVerifyIsAd(false));
  EXPECT_EQ(0, GetFilteredLoadCallCount());

  EXPECT_EQ(std::nullopt, CanRequestPreload());
  EXPECT_EQ(0, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextSubresourceFilterTest, DuringOnFreeze) {
  document->SetFreezingInProgress(true);
  // Only keepalive requests should succeed during onfreeze.
  EXPECT_EQ(ResourceRequestBlockedReason::kOther, CanRequest());
  EXPECT_EQ(std::nullopt, CanRequestKeepAlive());
  document->SetFreezingInProgress(false);
  EXPECT_EQ(std::nullopt, CanRequest());
  EXPECT_EQ(std::nullopt, CanRequestKeepAlive());
}

TEST_F(FrameFetchContextSubresourceFilterTest, WouldDisallow) {
  SetFilterPolicy(WebDocumentSubresourceFilter::kWouldDisallow);

  EXPECT_EQ(std::nullopt, CanRequestAndVerifyIsAd(true));
  EXPECT_EQ(0, GetFilteredLoadCallCount());

  EXPECT_EQ(std::nullopt, CanRequestPreload());
  EXPECT_EQ(0, GetFilteredLoadCallCount());
}

TEST_F(FrameFetchContextTest, AddAdditionalRequestHeadersWhenDetached) {
  const KURL document_url("https://www2.example.com/fuga/hoge.html");
  const String origin = "https://www2.example.com";
  ResourceRequest request(KURL("https://localhost/"));
  request.SetHttpMethod(http_names::kPUT);

  GetNetworkStateNotifier().SetSaveDataEnabledOverride(true);

  dummy_page_holder = nullptr;

  GetFetchContext()->AddAdditionalRequestHeaders(request);

  EXPECT_EQ(String(), request.HttpHeaderField(http_names::kSaveData));
}

TEST_F(FrameFetchContextTest, ResourceRequestCachePolicyWhenDetached) {
  ResourceRequest request(KURL("https://localhost/"));

  dummy_page_holder = nullptr;

  EXPECT_EQ(mojom::FetchCacheMode::kDefault,
            GetFetchContext()->ResourceRequestCachePolicy(
                request, ResourceType::kRaw, FetchParameters::kNoDefer));
}

TEST_F(FrameFetchContextMockedLocalFrameClientTest,
       PrepareRequestWhenDetached) {
  Checkpoint checkpoint;

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, UserAgent()).WillOnce(testing::Return(String("hi")));
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(1);
  dummy_page_holder = nullptr;
  checkpoint.Call(2);

  ResourceRequest request(KURL("https://localhost/"));
  WebScopedVirtualTimePauser virtual_time_pauser;
  ResourceLoaderOptions options(nullptr /* world */);
  GetFetchContext()->PrepareRequest(request, options, virtual_time_pauser,
                                    ResourceType::kRaw);

  EXPECT_EQ("hi", request.HttpHeaderField(http_names::kUserAgent));
}

TEST_F(FrameFetchContextTest, PrepareRequestHistogramCount) {
  ResourceRequest request(KURL("https://localhost/"));
  // Sets Sec-CH-UA-Reduced, which should result in the reduced User-Agent
  // string being used.
  request.SetHttpHeaderField(AtomicString("Sec-CH-ua-reduced"),
                             AtomicString("?1"));
  WebScopedVirtualTimePauser virtual_time_pauser;
  ResourceLoaderOptions options(nullptr /* world */);
  GetFetchContext()->PrepareRequest(request, options, virtual_time_pauser,
                                    ResourceType::kRaw);
}

TEST_F(FrameFetchContextTest, AddResourceTimingWhenDetached) {
  mojom::blink::ResourceTimingInfoPtr info = CreateResourceTimingInfo(
      base::TimeTicks() + base::Seconds(0.3), KURL(), nullptr);

  dummy_page_holder = nullptr;

  GetFetchContext()->AddResourceTiming(std::move(info), AtomicString("type"));
  // Should not crash.
}

TEST_F(FrameFetchContextTest, AllowImageWhenDetached) {
  const KURL url("https://www.example.com/");

  dummy_page_holder = nullptr;

  EXPECT_TRUE(GetFetchContext()->AllowImage());
}

TEST_F(FrameFetchContextTest, PopulateResourceRequestWhenDetached) {
  const KURL url("https://www.example.com/");
  ResourceRequest request(url);

  ResourceLoaderOptions options(nullptr /* world */);

  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kDeviceMemory);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kDpr);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED);
  document->GetFrame()->GetClientHintsPreferences().SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth);

  dummy_page_holder = nullptr;

  GetFetchContext()->UpgradeResourceRequestForLoader(
      ResourceType::kRaw, std::nullopt /* resource_width */, request, options);
  // Should not crash.
}

TEST_F(FrameFetchContextTest, SetFirstPartyCookieWhenDetached) {
  const KURL document_url("https://www2.example.com/foo/bar");
  RecreateFetchContext(document_url);

  const KURL url("https://www.example.com/hoge/fuga");
  ResourceRequest request(url);

  dummy_page_holder = nullptr;

  SetFirstPartyCookie(request);

  EXPECT_TRUE(request.SiteForCookies().IsEquivalent(
      net::SiteForCookies::FromUrl(GURL(document_url))));
}

TEST_F(FrameFetchContextTest, TopFrameOrigin) {
  const KURL document_url("https://www2.example.com/foo/bar");
  RecreateFetchContext(document_url);
  const SecurityOrigin* origin = document->domWindow()->GetSecurityOrigin();

  const KURL url("https://www.example.com/hoge/fuga");
  ResourceRequest request(url);

  EXPECT_EQ(origin, GetTopFrameOrigin());
}

TEST_F(FrameFetchContextTest, TopFrameOriginDetached) {
  const KURL document_url("https://www2.example.com/foo/bar");
  RecreateFetchContext(document_url);
  const SecurityOrigin* origin = document->domWindow()->GetSecurityOrigin();

  const KURL url("https://www.example.com/hoge/fuga");
  ResourceRequest request(url);

  dummy_page_holder = nullptr;

  EXPECT_EQ(origin, GetTopFrameOrigin());
}

// Tests that CanRequestCanRequestBasedOnSubresourceFilterOnly will block ads
// or not correctly, depending on the FilterPolicy.
TEST_F(FrameFetchContextSubresourceFilterTest,
       CanRequestBasedOnSubresourceFilterOnly) {
  const struct {
    WebDocumentSubresourceFilter::LoadPolicy policy;
    std::optional<ResourceRequestBlockedReason> expected_block_reason;
  } kTestCases[] = {
      {WebDocumentSubresourceFilter::kDisallow,
       ResourceRequestBlockedReason::kSubresourceFilter},
      {WebDocumentSubresourceFilter::kWouldDisallow, std::nullopt},
      {WebDocumentSubresourceFilter::kAllow, std::nullopt}};

  for (const auto& test : kTestCases) {
    SetFilterPolicy(test.policy);

    KURL url("http://ads.com/some_script.js");
    ResourceRequest resource_request(url);
    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::SCRIPT);
    resource_request.SetRequestorOrigin(GetTopFrameOrigin());

    ResourceLoaderOptions options(nullptr /* world */);

    EXPECT_EQ(test.expected_block_reason,
              GetFetchContext()->CanRequestBasedOnSubresourceFilterOnly(
                  ResourceType::kScript, resource_request, url, options,
                  ReportingDisposition::kReport, std::nullopt));
  }
}

// Tests that CalculateIfAdSubresource with an alias URL will tag ads
// correctly according to the SubresourceFilter mode.
TEST_F(FrameFetchContextSubresourceFilterTest,
       CalculateIfAdSubresourceWithAliasURL) {
  const struct {
    WebDocumentSubresourceFilter::LoadPolicy policy;
    bool expected_to_be_tagged_ad;
  } kTestCases[] = {{WebDocumentSubresourceFilter::kDisallow, true},
                    {WebDocumentSubresourceFilter::kWouldDisallow, true},
                    {WebDocumentSubresourceFilter::kAllow, false}};

  for (const auto& test : kTestCases) {
    SetFilterPolicy(test.policy);

    KURL url("http://www.example.com");
    KURL alias_url("http://ads.com/some_script.js");
    ResourceRequest resource_request(url);
    resource_request.SetRequestContext(
        mojom::blink::RequestContextType::SCRIPT);
    resource_request.SetRequestorOrigin(GetTopFrameOrigin());

    ResourceLoaderOptions options(nullptr /* world */);

    EXPECT_EQ(test.expected_to_be_tagged_ad,
              GetFetchContext()->CalculateIfAdSubresource(
                  resource_request, alias_url, ResourceType::kScript,
                  options.initiator_info));
  }
}

class FrameFetchContextDisableReduceAcceptLanguageTest
    : public FrameFetchContextTest,
      public testing::WithParamInterface<bool> {
 public:
  FrameFetchContextDisableReduceAcceptLanguageTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{network::features::kReduceAcceptLanguage});
  }

 protected:
  void SetupForAcceptLanguageTest(bool is_detached, ResourceRequest& request) {
    ResourceLoaderOptions options(/*world=*/nullptr);

    document->GetFrame()->SetReducedAcceptLanguage(AtomicString("en-GB"));

    if (is_detached)
      dummy_page_holder = nullptr;

    GetFetchContext()->UpgradeResourceRequestForLoader(
        ResourceType::kRaw, std::nullopt /* resource_width */, request,
        options);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ReduceAcceptLanguage,
                         FrameFetchContextDisableReduceAcceptLanguageTest,
                         testing::Bool());

TEST_P(FrameFetchContextDisableReduceAcceptLanguageTest,
       VerifyReduceAcceptLanguage) {
  const KURL url("https://www.example.com/");
  ResourceRequest request(url);
  SetupForAcceptLanguageTest(/*is_detached=*/GetParam(), request);
  // Expect no Accept-Language header set when feature is disabled.
  EXPECT_EQ(nullptr, request.HttpHeaderField(http_names::kAcceptLanguage));
}

}  // namespace blink
