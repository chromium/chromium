// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_resource_preloader.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html/parser/preload_request.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

struct PreloadScannerTestCase {
  const char* base_url;
  const char* input_html;
  const char* preloaded_url;  // Or nullptr if no preload is expected.
  const char* output_base_url;
  ResourceType type;
  int resource_width;
  ClientHintsPreferences preferences;
};

struct RenderBlockingTestCase {
  const char* base_url;
  const char* input_html;
  RenderBlockingBehavior renderBlocking;
};

struct HTMLPreconnectTestCase {
  const char* base_url;
  const char* input_html;
  const char* preconnected_host;
  CrossOriginAttributeValue cross_origin;
};

struct ReferrerPolicyTestCase {
  const char* base_url;
  const char* input_html;
  const char* preloaded_url;  // Or nullptr if no preload is expected.
  const char* output_base_url;
  ResourceType type;
  int resource_width;
  network::mojom::ReferrerPolicy referrer_policy;
  // Expected referrer header of the preload request, or nullptr if the header
  // shouldn't be checked (and no network request should be created).
  const char* expected_referrer;
};

struct CorsTestCase {
  const char* base_url;
  const char* input_html;
  network::mojom::RequestMode request_mode;
  network::mojom::CredentialsMode credentials_mode;
};

struct CSPTestCase {
  const char* base_url;
  const char* input_html;
  bool should_see_csp_tag;
};

struct NonceTestCase {
  const char* base_url;
  const char* input_html;
  const char* nonce;
};

struct ContextTestCase {
  const char* base_url;
  const char* input_html;
  const char* preloaded_url;  // Or nullptr if no preload is expected.
  bool is_image_set;
};

struct IntegrityTestCase {
  size_t number_of_integrity_metadata_found;
  const char* input_html;
};

struct LazyLoadImageTestCase {
  const char* input_html;
  bool should_preload;
};

struct AttributionSrcTestCase {
  bool use_secure_document_url;
  const char* base_url;
  const char* input_html;
  network::mojom::AttributionReportingEligibility expected_eligibility;
  network::mojom::AttributionSupport attribution_support =
      network::mojom::AttributionSupport::kWeb;
};

struct TokenStreamMatcherTestCase {
  ElementLocator locator;
  const char* input_html;
  const char* potentially_lcp_preload_url;
  bool should_preload;
};

struct SharedStorageWritableTestCase {
  bool use_secure_document_url;
  const char* base_url;
  const char* input_html;
  bool expected_shared_storage_writable_opted_in;
};

class HTMLMockHTMLResourcePreloader : public ResourcePreloader {
 public:
  explicit HTMLMockHTMLResourcePreloader(const KURL& document_url)
      : document_url_(document_url) {}

  void TakePreloadData(std::unique_ptr<PendingPreloadData> preload_data) {
    preload_data_ = std::move(preload_data);
    TakeAndPreload(preload_data_->requests);
  }

  void PreloadRequestVerification(ResourceType type,
                                  const char* url,
                                  const char* base_url,
                                  int width,
                                  const ClientHintsPreferences& preferences) {
    if (!url) {
      EXPECT_FALSE(preload_request_) << preload_request_->ResourceURL();
      return;
    }
    EXPECT_NE(nullptr, preload_request_.get());
    if (preload_request_) {
      EXPECT_FALSE(preload_request_->IsPreconnect());
      EXPECT_EQ(type, preload_request_->GetResourceType());
      EXPECT_EQ(url, preload_request_->ResourceURL());
      EXPECT_EQ(base_url, preload_request_->BaseURL().GetString());
      EXPECT_EQ(width, preload_request_->GetResourceWidth().value_or(0));

      ClientHintsPreferences preload_preferences;
      for (const auto& value : preload_data_->meta_ch_values) {
        preload_preferences.UpdateFromMetaCH(value.value, document_url_,
                                             nullptr, value.type,
                                             value.is_doc_preloader,
                                             /*is_sync_parser=*/false);
      }
      EXPECT_EQ(preferences.ShouldSend(
                    network::mojom::WebClientHintsType::kDpr_DEPRECATED),
                preload_preferences.ShouldSend(
                    network::mojom::WebClientHintsType::kDpr_DEPRECATED));
      EXPECT_EQ(
          preferences.ShouldSend(network::mojom::WebClientHintsType::kDpr),
          preload_preferences.ShouldSend(
              network::mojom::WebClientHintsType::kDpr));
      EXPECT_EQ(
          preferences.ShouldSend(
              network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED),
          preload_preferences.ShouldSend(
              network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED));
      EXPECT_EQ(preferences.ShouldSend(
                    network::mojom::WebClientHintsType::kResourceWidth),
                preload_preferences.ShouldSend(
                    network::mojom::WebClientHintsType::kResourceWidth));
      EXPECT_EQ(
          preferences.ShouldSend(
              network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED),
          preload_preferences.ShouldSend(
              network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED));
      EXPECT_EQ(preferences.ShouldSend(
                    network::mojom::WebClientHintsType::kViewportWidth),
                preload_preferences.ShouldSend(
                    network::mojom::WebClientHintsType::kViewportWidth));
    }
  }

  void PreloadRequestVerification(
      ResourceType type,
      const char* url,
      const char* base_url,
      int width,
      network::mojom::ReferrerPolicy referrer_policy) {
    PreloadRequestVerification(type, url, base_url, width,
                               ClientHintsPreferences());
    EXPECT_EQ(referrer_policy, preload_request_->GetReferrerPolicy());
  }

  void PreloadRequestVerification(
      ResourceType type,
      const char* url,
      const char* base_url,
      int width,
      network::mojom::ReferrerPolicy referrer_policy,
      Document* document,
      const char* expected_referrer) {
    PreloadRequestVerification(type, url, base_url, width, referrer_policy);
    Resource* resource = preload_request_->Start(document);
    ASSERT_TRUE(resource);
    EXPECT_EQ(expected_referrer,
              resource->GetResourceRequest().ReferrerString());
  }

  void RenderBlockingRequestVerification(
      RenderBlockingBehavior renderBlocking) {
    ASSERT_TRUE(preload_request_);
    EXPECT_EQ(preload_request_->GetRenderBlockingBehavior(), renderBlocking);
  }

  void PreconnectRequestVerification(const String& host,
                                     CrossOriginAttributeValue cross_origin) {
    if (!host.IsNull()) {
      EXPECT_TRUE(preload_request_->IsPreconnect());
      EXPECT_EQ(preload_request_->ResourceURL(), host);
      EXPECT_EQ(preload_request_->CrossOrigin(), cross_origin);
    }
  }

  void CorsRequestVerification(
      Document* document,
      network::mojom::RequestMode request_mode,
      network::mojom::CredentialsMode credentials_mode) {
    ASSERT_TRUE(preload_request_.get());
    Resource* resource = preload_request_->Start(document);
    ASSERT_TRUE(resource);
    EXPECT_EQ(request_mode, resource->GetResourceRequest().GetMode());
    EXPECT_EQ(credentials_mode,
              resource->GetResourceRequest().GetCredentialsMode());
  }

  void NonceRequestVerification(const char* nonce) {
    ASSERT_TRUE(preload_request_.get());
    if (strlen(nonce))
      EXPECT_EQ(nonce, preload_request_->Nonce());
    else
      EXPECT_TRUE(preload_request_->Nonce().empty());
  }

  void ContextVerification(bool is_image_set) {
    ASSERT_TRUE(preload_request_.get());
    EXPECT_EQ(preload_request_->IsImageSetForTestingOnly(), is_image_set);
  }

  void CheckNumberOfIntegrityConstraints(size_t expected) {
    size_t actual = 0;
    if (preload_request_) {
      actual = preload_request_->IntegrityMetadataForTestingOnly().size();
      EXPECT_EQ(expected, actual);
    }
  }

  void LazyLoadImagePreloadVerification(bool expected) {
    if (expected) {
      EXPECT_TRUE(preload_request_.get());
    } else {
      EXPECT_FALSE(preload_request_) << preload_request_->ResourceURL();
    }
  }

  void AttributionSrcRequestVerification(
      Document* document,
      network::mojom::AttributionReportingEligibility expected_eligibility,
      network::mojom::AttributionSupport expected_support) {
    ASSERT_TRUE(preload_request_.get());
    Resource* resource = preload_request_->Start(document);
    ASSERT_TRUE(resource);

    EXPECT_EQ(
        expected_eligibility,
        resource->GetResourceRequest().GetAttributionReportingEligibility());

    EXPECT_EQ(expected_support,
              resource->GetResourceRequest().GetAttributionReportingSupport());
  }

  void IsPotentiallyLCPElementFlagVerification(bool expected) {
    EXPECT_EQ(expected, preload_request_->IsPotentiallyLCPElement())
        << preload_request_->ResourceURL();
  }

  void SharedStorageWritableRequestVerification(
      Document* document,
      bool expected_shared_storage_writable_opted_in) {
    ASSERT_TRUE(preload_request_.get());
    Resource* resource = preload_request_->Start(document);
    ASSERT_TRUE(resource);

    EXPECT_EQ(expected_shared_storage_writable_opted_in,
              resource->GetResourceRequest().GetSharedStorageWritableOptedIn());
  }

 protected:
  void Preload(std::unique_ptr<PreloadRequest> preload_request) override {
    preload_request_ = std::move(preload_request);
  }

 private:
  std::unique_ptr<PreloadRequest> preload_request_;
  std::unique_ptr<PendingPreloadData> preload_data_;
  KURL document_url_;
};

class HTMLPreloadScannerTest : public PageTestBase {
 protected:
  enum ViewportState {
    kViewportEnabled,
    kViewportDisabled,
  };

  enum PreloadState {
    kPreloadEnabled,
    kPreloadDisabled,
  };

  std::unique_ptr<MediaValuesCached::MediaValuesCachedData>
  CreateMediaValuesData() {
    auto data = std::make_unique<MediaValuesCached::MediaValuesCachedData>();
    data->viewport_width = 500;
    data->viewport_height = 600;
    data->device_width = 700;
    data->device_height = 800;
    data->device_pixel_ratio = 2.0;
    data->color_bits_per_component = 24;
    data->monochrome_bits_per_component = 0;
    data->primary_pointer_type = mojom::blink::PointerType::kPointerFineType;
    data->three_d_enabled = true;
    data->media_type = media_type_names::kScreen;
    data->strict_mode = true;
    data->display_mode = blink::mojom::DisplayMode::kBrowser;
    return data;
  }

  void RunSetUp(ViewportState viewport_state,
                PreloadState preload_state = kPreloadEnabled,
                network::mojom::ReferrerPolicy document_referrer_policy =
                    network::mojom::ReferrerPolicy::kDefault,
                bool use_secure_document_url = false,
                Vector<ElementLocator> locators = {},
                bool disable_preload_scanning = false) {
    HTMLParserOptions options(&GetDocument());
    KURL document_url = KURL("http://whatever.test/");
    if (use_secure_document_url)
      document_url = KURL("https://whatever.test/");
    NavigateTo(document_url);
    GetDocument().GetSettings()->SetViewportEnabled(viewport_state ==
                                                    kViewportEnabled);
    GetDocument().GetSettings()->SetViewportMetaEnabled(viewport_state ==
                                                        kViewportEnabled);
    GetDocument().GetSettings()->SetDoHtmlPreloadScanning(preload_state ==
                                                          kPreloadEnabled);
    GetFrame().DomWindow()->SetReferrerPolicy(document_referrer_policy);
    scanner_ = std::make_unique<HTMLPreloadScanner>(
        std::make_unique<HTMLTokenizer>(options), document_url,
        std::make_unique<CachedDocumentParameters>(&GetDocument()),
        CreateMediaValuesData(),
        TokenPreloadScanner::ScannerType::kMainDocument,
        /* script_token_scanner=*/nullptr,
        /* take_preload=*/HTMLPreloadScanner::TakePreloadFn(), locators,
        disable_preload_scanning);
  }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    RunSetUp(kViewportEnabled);
  }

  void Test(PreloadScannerTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));

    preloader.PreloadRequestVerification(
        test_case.type, test_case.preloaded_url, test_case.output_base_url,
        test_case.resource_width, test_case.preferences);
  }

  void Test(RenderBlockingTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    RunSetUp(kViewportEnabled, kPreloadEnabled,
             network::mojom::ReferrerPolicy::kDefault, true);
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));
    preloader.RenderBlockingRequestVerification(test_case.renderBlocking);
  }

  void Test(HTMLPreconnectTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));
    preloader.PreconnectRequestVerification(test_case.preconnected_host,
                                            test_case.cross_origin);
  }

  void Test(ReferrerPolicyTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));

    if (test_case.expected_referrer) {
      preloader.PreloadRequestVerification(
          test_case.type, test_case.preloaded_url, test_case.output_base_url,
          test_case.resource_width, test_case.referrer_policy, &GetDocument(),
          test_case.expected_referrer);
    } else {
      preloader.PreloadRequestVerification(
          test_case.type, test_case.preloaded_url, test_case.output_base_url,
          test_case.resource_width, test_case.referrer_policy);
    }
  }

  void Test(CorsTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));
    preloader.CorsRequestVerification(&GetDocument(), test_case.request_mode,
                                      test_case.credentials_mode);
  }

  void Test(CSPTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    auto data = scanner_->Scan(base_url);
    EXPECT_EQ(test_case.should_see_csp_tag, data->has_csp_meta_tag);
  }

  void Test(NonceTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));
    preloader.NonceRequestVerification(test_case.nonce);
  }

  void Test(ContextTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));

    preloader.ContextVerification(test_case.is_image_set);
  }

  void Test(IntegrityTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url("http://example.test/");
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));

    preloader.CheckNumberOfIntegrityConstraints(
        test_case.number_of_integrity_metadata_found);
  }

  void Test(LazyLoadImageTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url("http://example.test/");
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));
    preloader.LazyLoadImagePreloadVerification(test_case.should_preload);
  }

  void Test(AttributionSrcTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);

    GetPage().SetAttributionSupport(test_case.attribution_support);

    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));
    preloader.AttributionSrcRequestVerification(&GetDocument(),
                                                test_case.expected_eligibility,
                                                test_case.attribution_support);
  }

  void Test(TokenStreamMatcherTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    RunSetUp(kViewportEnabled, kPreloadEnabled,
             network::mojom::ReferrerPolicy::kDefault,
             /* use_secure_document_url=*/true, {test_case.locator});
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data =
        scanner_->Scan(GetDocument().Url());
    int count = 0;
    for (const auto& request_ptr : preload_data->requests) {
      if (request_ptr->IsPotentiallyLCPElement()) {
        EXPECT_EQ(request_ptr->ResourceURL(),
                  String(test_case.potentially_lcp_preload_url));
        count++;
      }
    }

    EXPECT_EQ(test_case.should_preload ? 1 : 0, count);
  }

  void Test(SharedStorageWritableTestCase test_case) {
    SCOPED_TRACE(base::StringPrintf("Use secure doc URL: %d; HTML: '%s'",
                                    test_case.use_secure_document_url,
                                    test_case.input_html));

    HTMLMockHTMLResourcePreloader preloader(GetDocument().Url());
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    std::unique_ptr<PendingPreloadData> preload_data = scanner_->Scan(base_url);
    preloader.TakePreloadData(std::move(preload_data));
    preloader.SharedStorageWritableRequestVerification(
        &GetDocument(), test_case.expected_shared_storage_writable_opted_in);
  }

 private:
  std::unique_ptr<HTMLPreloadScanner> scanner_;
};

TEST_F(HTMLPreloadScannerTest, testImages) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test", "<img src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>",
       "bla3.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif "
       "500w'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewport) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport content='width=160'><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w'>",
       "bla2.gif", "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla2.gif", "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla2.gif", "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla2.gif", "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla2.gif", "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla2.gif", "http://example.test/", ResourceType::kImage, 80},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla2.gif", "http://example.test/", ResourceType::kImage, 80},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewportDeviceWidth) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport content='width=device-width'><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w'>",
       "bla3.gif", "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 350},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 350},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewportDisabled) {
  RunSetUp(kViewportDisabled);
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport content='width=160'><img src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif",
       "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>",
       "bla3.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", ResourceType::kImage, 250},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testViewportNoContent) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport><img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<meta name=viewport content=sdkbsdkjnejjha><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testMetaAcceptCH) {
  ClientHintsPreferences dpr_DEPRECATED;
  ClientHintsPreferences dpr;
  ClientHintsPreferences resource_width_DEPRECATED;
  ClientHintsPreferences resource_width;
  ClientHintsPreferences all;
  ClientHintsPreferences viewport_width_DEPRECATED;
  ClientHintsPreferences viewport_width;
  dpr_DEPRECATED.SetShouldSend(
      network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  dpr.SetShouldSend(network::mojom::WebClientHintsType::kDpr);
  all.SetShouldSend(network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  all.SetShouldSend(network::mojom::WebClientHintsType::kDpr);
  resource_width_DEPRECATED.SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED);
  resource_width.SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth);
  all.SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED);
  all.SetShouldSend(network::mojom::WebClientHintsType::kResourceWidth);
  viewport_width_DEPRECATED.SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED);
  viewport_width.SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth);
  all.SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED);
  all.SetShouldSend(network::mojom::WebClientHintsType::kViewportWidth);
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='bla'><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='dprw'><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<meta http-equiv='accept-ch'><img srcset='bla.gif 320w, blabla.gif "
       "640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='dpr  '><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       dpr_DEPRECATED},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='sec-ch-dpr  '><img "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0, dpr},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='bla,dpr  '><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       dpr_DEPRECATED},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='bla,sec-ch-dpr  '><img "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0, dpr},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  width  '><img sizes='100vw' "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 500,
       resource_width_DEPRECATED},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  sec-ch-width  '><img "
       "sizes='100vw' srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 500,
       resource_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  width  , wutever'><img "
       "sizes='300px' srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 300,
       resource_width_DEPRECATED},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  sec-ch-width  , wutever'><img "
       "sizes='300px' srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 300,
       resource_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  '><img "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       viewport_width_DEPRECATED},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  sec-ch-viewport-width  '><img "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       viewport_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  , "
       "wutever'><img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       viewport_width_DEPRECATED},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  sec-ch-viewport-width  , "
       "wutever'><img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       viewport_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  ,width, "
       "wutever, dpr , sec-ch-dpr,sec-ch-viewport-width,   sec-ch-width '><img "
       "sizes='90vw' srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 450, all},
  };

  for (const auto& test_case : test_cases) {
    RunSetUp(kViewportDisabled, kPreloadEnabled,
             network::mojom::ReferrerPolicy::kDefault,
             true /* use_secure_document_url */);
    Test(test_case);
  }
}

TEST_F(HTMLPreloadScannerTest, testMetaAcceptCHInsecureDocument) {
  ClientHintsPreferences all;
  all.SetShouldSend(network::mojom::WebClientHintsType::kDpr_DEPRECATED);
  all.SetShouldSend(network::mojom::WebClientHintsType::kDpr);
  all.SetShouldSend(
      network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED);
  all.SetShouldSend(network::mojom::WebClientHintsType::kResourceWidth);
  all.SetShouldSend(
      network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED);
  all.SetShouldSend(network::mojom::WebClientHintsType::kViewportWidth);

  const PreloadScannerTestCase expect_no_client_hint = {
      "http://example.test",
      "<meta http-equiv='accept-ch' content='  viewport-width  ,width, "
      "wutever, dpr  '><img sizes='90vw' srcset='bla.gif 320w, blabla.gif "
      "640w'>",
      "blabla.gif",
      "http://example.test/",
      ResourceType::kImage,
      450};

  const PreloadScannerTestCase expect_client_hint = {
      "http://example.test",
      "<meta http-equiv='accept-ch' content='  viewport-width  ,width, "
      "wutever, dpr,   sec-ch-viewport-width  ,sec-ch-width, wutever2, "
      "sec-ch-dpr  '><img sizes='90vw' srcset='bla.gif 320w, blabla.gif 640w'>",
      "blabla.gif",
      "http://example.test/",
      ResourceType::kImage,
      450,
      all};

  // For an insecure document, client hint should not be attached.
  RunSetUp(kViewportDisabled, kPreloadEnabled,
           network::mojom::ReferrerPolicy::kDefault,
           false /* use_secure_document_url */);
  Test(expect_no_client_hint);

  // For a secure document, client hint should be attached.
  RunSetUp(kViewportDisabled, kPreloadEnabled,
           network::mojom::ReferrerPolicy::kDefault,
           true /* use_secure_document_url */);
  Test(expect_client_hint);
}

TEST_F(HTMLPreloadScannerTest, testRenderBlocking) {
  RenderBlockingTestCase test_cases[] = {
      {"http://example.test", "<link rel=preload href='bla.gif' as=image>",
       RenderBlockingBehavior::kNonBlocking},
      {"http://example.test",
       "<script type='module' src='test.js' defer></script>",
       RenderBlockingBehavior::kNonBlocking},
      {"http://example.test",
       "<script type='module' src='test.js' async></script>",
       RenderBlockingBehavior::kPotentiallyBlocking},
      {"http://example.test",
       "<script type='module' src='test.js' defer blocking='render'></script>",
       RenderBlockingBehavior::kBlocking},
      {"http://example.test", "<script src='test.js'></script>",
       RenderBlockingBehavior::kBlocking},
      {"http://example.test", "<body><script src='test.js'></script></body>",
       RenderBlockingBehavior::kInBodyParserBlocking},
      {"http://example.test", "<script src='test.js' disabled></script>",
       RenderBlockingBehavior::kBlocking},
      {"http://example.test", "<link rel=stylesheet href=http://example2.test>",
       RenderBlockingBehavior::kBlocking},
      {"http://example.test",
       "<body><link rel=stylesheet href=http://example2.test></body>",
       RenderBlockingBehavior::kInBodyParserBlocking},
      {"http://example.test",
       "<link rel=stylesheet href=http://example2.test disabled>",
       RenderBlockingBehavior::kNonBlocking},
  };

  for (const auto& test_case : test_cases) {
    Test(test_case);
  }
}

TEST_F(HTMLPreloadScannerTest, testPreconnect) {
  HTMLPreconnectTestCase test_cases[] = {
      {"http://example.test", "<link rel=preconnect href=http://example2.test>",
       "http://example2.test", kCrossOriginAttributeNotSet},
      {"http://example.test",
       "<link rel=preconnect href=http://example2.test crossorigin=anonymous>",
       "http://example2.test", kCrossOriginAttributeAnonymous},
      {"http://example.test",
       "<link rel=preconnect href=http://example2.test "
       "crossorigin='use-credentials'>",
       "http://example2.test", kCrossOriginAttributeUseCredentials},
      {"http://example.test",
       "<link rel=preconnected href=http://example2.test "
       "crossorigin='use-credentials'>",
       nullptr, kCrossOriginAttributeNotSet},
      {"http://example.test",
       "<link rel=preconnect href=ws://example2.test "
       "crossorigin='use-credentials'>",
       nullptr, kCrossOriginAttributeNotSet},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testDisables) {
  RunSetUp(kViewportEnabled, kPreloadDisabled);

  PreloadScannerTestCase test_cases[] = {
      {"http://example.test", "<img src='bla.gif'>"},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testPicture) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif'><img src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif' type=''><img "
       "src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<picture><source sizes='50vw' srcset='srcset_bla.gif'><img "
       "src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source sizes='50vw' srcset='srcset_bla.gif'><img "
       "sizes='50vw' src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif' sizes='50vw'><img "
       "sizes='50vw' src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif'><img sizes='50vw' "
       "src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<picture><source media='(max-width: 900px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<picture><source media='(max-width: 400px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source type='image/webp' srcset='srcset_bla.gif'><img "
       "sizes='50vw' srcset='bla.gif 500w'></picture>",
       "srcset_bla.gif", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<picture><source type='image/jp2' srcset='srcset_bla.gif'><img "
       "sizes='50vw' srcset='bla.gif 500w'></picture>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source media='(max-width: 900px)' type='image/jp2' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source type='image/webp' media='(max-width: 400px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source type='image/jp2' media='(max-width: 900px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 250},
      {"http://example.test",
       "<picture><source media='(max-width: 400px)' type='image/webp' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 250},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testContext) {
  ContextTestCase test_cases[] = {
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif'><img src='bla.gif'></picture>",
       "srcset_bla.gif", true},
      {"http://example.test", "<img src='bla.gif'>", "bla.gif", false},
      {"http://example.test", "<img srcset='bla.gif'>", "bla.gif", true},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testReferrerPolicy) {
  ReferrerPolicyTestCase test_cases[] = {
      {"http://example.test", "<img src='bla.gif'/>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kDefault},
      {"http://example.test", "<img referrerpolicy='origin' src='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kOrigin, nullptr},
      {"http://example.test",
       "<meta name='referrer' content='not-a-valid-policy'><img "
       "src='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kDefault, nullptr},
      {"http://example.test",
       "<img referrerpolicy='origin' referrerpolicy='origin-when-cross-origin' "
       "src='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kOrigin, nullptr},
      {"http://example.test",
       "<img referrerpolicy='not-a-valid-policy' src='bla.gif'/>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kDefault, nullptr},
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='origin-when-cross-origin' "
       "href='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, nullptr},
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='same-origin' "
       "href='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kSameOrigin, nullptr},
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='strict-origin' "
       "href='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kStrictOrigin, nullptr},
      {"http://example.test",
       "<link rel=preload as=image "
       "referrerpolicy='strict-origin-when-cross-origin' "
       "href='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin, nullptr},
      {"http://example.test",
       "<link rel='stylesheet' href='sheet.css' type='text/css'>", "sheet.css",
       "http://example.test/", ResourceType::kCSSStyleSheet, 0,
       network::mojom::ReferrerPolicy::kDefault, nullptr},
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='origin' "
       "referrerpolicy='origin-when-cross-origin' href='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kOrigin, nullptr},
      {"http://example.test",
       "<meta name='referrer' content='no-referrer'><img "
       "referrerpolicy='origin' src='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kOrigin, nullptr},
      // The scanner's state is not reset between test cases, so all subsequent
      // test cases have a document referrer policy of no-referrer.
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='not-a-valid-policy' "
       "href='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kNever, nullptr},
      {"http://example.test",
       "<img referrerpolicy='not-a-valid-policy' src='bla.gif'/>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kNever, nullptr},
      {"http://example.test", "<img src='bla.gif'/>", "bla.gif",
       "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kNever, nullptr}};

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testCors) {
  CorsTestCase test_cases[] = {
      {"http://example.test", "<script src='/script'></script>",
       network::mojom::RequestMode::kNoCors,
       network::mojom::CredentialsMode::kInclude},
      {"http://example.test", "<script crossorigin src='/script'></script>",
       network::mojom::RequestMode::kCors,
       network::mojom::CredentialsMode::kSameOrigin},
      {"http://example.test",
       "<script crossorigin=use-credentials src='/script'></script>",
       network::mojom::RequestMode::kCors,
       network::mojom::CredentialsMode::kInclude},
      {"http://example.test", "<script type='module' src='/script'></script>",
       network::mojom::RequestMode::kCors,
       network::mojom::CredentialsMode::kSameOrigin},
      {"http://example.test",
       "<script type='module' crossorigin='anonymous' src='/script'></script>",
       network::mojom::RequestMode::kCors,
       network::mojom::CredentialsMode::kSameOrigin},
      {"http://example.test",
       "<script type='module' crossorigin='use-credentials' "
       "src='/script'></script>",
       network::mojom::RequestMode::kCors,
       network::mojom::CredentialsMode::kInclude},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.input_html);
    Test(test_case);
  }
}

TEST_F(HTMLPreloadScannerTest, testCSP) {
  CSPTestCase test_cases[] = {
      {"http://example.test",
       "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
       "https:\">",
       true},
      {"http://example.test",
       "<meta name=\"viewport\" content=\"width=device-width\">", false},
      {"http://example.test", "<img src=\"example.gif\">", false}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.input_html);
    Test(test_case);
  }
}

TEST_F(HTMLPreloadScannerTest, testNonce) {
  NonceTestCase test_cases[] = {
      {"http://example.test", "<script src='/script'></script>", ""},
      {"http://example.test", "<script src='/script' nonce=''></script>", ""},
      {"http://example.test", "<script src='/script' nonce='abc'></script>",
       "abc"},
      {"http://example.test", "<link rel='stylesheet' href='/style'>", ""},
      {"http://example.test", "<link rel='stylesheet' href='/style' nonce=''>",
       ""},
      {"http://example.test",
       "<link rel='stylesheet' href='/style' nonce='abc'>", "abc"},

      // <img> doesn't support nonces:
      {"http://example.test", "<img src='/image'>", ""},
      {"http://example.test", "<img src='/image' nonce=''>", ""},
      {"http://example.test", "<img src='/image' nonce='abc'>", ""},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.input_html);
    Test(test_case);
  }
}

TEST_F(HTMLPreloadScannerTest, testAttributionSrc) {
  static constexpr bool kSecureDocumentUrl = true;
  static constexpr bool kInsecureDocumentUrl = false;

  static constexpr char kSecureBaseURL[] = "https://example.test";
  static constexpr char kInsecureBaseURL[] = "http://example.test";

  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("https://example.test/script"), "");
  url_test_helpers::RegisterMockedURLLoad(
      url_test_helpers::ToKURL("http://example.test/script"), "");

  GetDocument().GetSettings()->SetScriptEnabled(true);

  AttributionSrcTestCase test_cases[] = {
      // Insecure context
      {kInsecureDocumentUrl, kSecureBaseURL,
       "<img src='/image' attributionsrc>",
       network::mojom::AttributionReportingEligibility::kUnset},
      {kInsecureDocumentUrl, kSecureBaseURL,
       "<script src='/script' attributionsrc></script>",
       network::mojom::AttributionReportingEligibility::kUnset},
      // No attributionsrc attribute
      {kSecureDocumentUrl, kSecureBaseURL, "<img src='/image'>",
       network::mojom::AttributionReportingEligibility::kUnset},
      {kSecureDocumentUrl, kSecureBaseURL, "<script src='/script'></script>",
       network::mojom::AttributionReportingEligibility::kUnset},
      // Irrelevant element type
      {kSecureDocumentUrl, kSecureBaseURL,
       "<video poster='/image' attributionsrc>",
       network::mojom::AttributionReportingEligibility::kUnset},
      // Not potentially trustworthy reporting origin
      {kSecureDocumentUrl, kInsecureBaseURL,
       "<img src='/image' attributionsrc>",
       network::mojom::AttributionReportingEligibility::kUnset},
      {kSecureDocumentUrl, kInsecureBaseURL,
       "<script src='/script' attributionsrc></script>",
       network::mojom::AttributionReportingEligibility::kUnset},
      // Secure context, potentially trustworthy reporting origin,
      // attributionsrc attribute
      {kSecureDocumentUrl, kSecureBaseURL, "<img src='/image' attributionsrc>",
       network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger},
      {kSecureDocumentUrl, kSecureBaseURL,
       "<script src='/script' attributionsrc></script>",
       network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger},
      {kSecureDocumentUrl, kSecureBaseURL, "<img src='/image' attributionsrc>",
       network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger,
       network::mojom::AttributionSupport::kWebAndOs},
  };

  for (const auto& test_case : test_cases) {
    RunSetUp(kViewportDisabled, kPreloadEnabled,
             network::mojom::ReferrerPolicy::kDefault,
             /*use_secure_document_url=*/test_case.use_secure_document_url);
    Test(test_case);
  }
}

// Tests that a document-level referrer policy (e.g. one set by HTTP header) is
// applied for preload requests.
TEST_F(HTMLPreloadScannerTest, testReferrerPolicyOnDocument) {
  RunSetUp(kViewportEnabled, kPreloadEnabled,
           network::mojom::ReferrerPolicy::kOrigin);
  ReferrerPolicyTestCase test_cases[] = {
      {"http://example.test", "<img src='blah.gif'/>", "blah.gif",
       "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kOrigin, nullptr},
      {"http://example.test", "<style>@import url('blah.css');</style>",
       "blah.css", "http://example.test/", ResourceType::kCSSStyleSheet, 0,
       network::mojom::ReferrerPolicy::kOrigin, nullptr},
      // Tests that a meta-delivered referrer policy with an unrecognized policy
      // value does not override the document's referrer policy.
      {"http://example.test",
       "<meta name='referrer' content='not-a-valid-policy'><img "
       "src='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kOrigin, nullptr},
      // Tests that a meta-delivered referrer policy with a valid policy value
      // does override the document's referrer policy.
      {"http://example.test",
       "<meta name='referrer' content='unsafe-url'><img src='bla.gif'/>",
       "bla.gif", "http://example.test/", ResourceType::kImage, 0,
       network::mojom::ReferrerPolicy::kAlways, nullptr},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testLinkRelPreload) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test", "<link rel=preload as=fetch href=bla>", "bla",
       "http://example.test/", ResourceType::kRaw, 0},
      {"http://example.test", "<link rel=preload href=bla as=script>", "bla",
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=script type='script/foo'>", "bla",
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test", "<link rel=preload href=bla as=style>", "bla",
       "http://example.test/", ResourceType::kCSSStyleSheet, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=style type='text/css'>", "bla",
       "http://example.test/", ResourceType::kCSSStyleSheet, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=style type='text/bla'>", nullptr,
       "http://example.test/", ResourceType::kCSSStyleSheet, 0},
      {"http://example.test", "<link rel=preload href=bla as=image>", "bla",
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image type='image/webp'>", "bla",
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image type='image/bla'>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<link rel=preload href=bla as=font>", "bla",
       "http://example.test/", ResourceType::kFont, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=font type='font/woff2'>", "bla",
       "http://example.test/", ResourceType::kFont, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=font type='font/bla'>", nullptr,
       "http://example.test/", ResourceType::kFont, 0},
      // Until the preload cache is defined in terms of range requests and media
      // fetches we can't reliably preload audio/video content and expect it to
      // be served from the cache correctly. Until
      // https://github.com/w3c/preload/issues/97 is resolved and implemented we
      // need to disable these preloads.
      {"http://example.test", "<link rel=preload href=bla as=video>", nullptr,
       "http://example.test/", ResourceType::kVideo, 0},
      {"http://example.test", "<link rel=preload href=bla as=track>", "bla",
       "http://example.test/", ResourceType::kTextTrack, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image media=\"(max-width: 800px)\">",
       "bla", "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image media=\"(max-width: 400px)\">",
       nullptr, "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<link rel=preload href=bla>", nullptr,
       "http://example.test/", ResourceType::kRaw, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testNoDataUrls) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<link rel=preload href='data:text/html,<p>data</data>'>", nullptr,
       "http://example.test/", ResourceType::kRaw, 0},
      {"http://example.test", "<img src='data:text/html,<p>data</data>'>",
       nullptr, "http://example.test/", ResourceType::kImage, 0},
      {"data:text/html,<a>anchor</a>", "<img src='#anchor'>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

// The preload scanner should follow the same policy that the ScriptLoader does
// with regard to the type and language attribute.
TEST_F(HTMLPreloadScannerTest, testScriptTypeAndLanguage) {
  PreloadScannerTestCase test_cases[] = {
      // Allow empty src and language attributes.
      {"http://example.test", "<script src='test.js'></script>", "test.js",
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test",
       "<script type='' language='' src='test.js'></script>", "test.js",
       "http://example.test/", ResourceType::kScript, 0},
      // Allow standard language and type attributes.
      {"http://example.test",
       "<script type='text/javascript' src='test.js'></script>", "test.js",
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test",
       "<script type='text/javascript' language='javascript' "
       "src='test.js'></script>",
       "test.js", "http://example.test/", ResourceType::kScript, 0},
      // Allow legacy languages in the "language" attribute with an empty
      // type.
      {"http://example.test",
       "<script language='javascript1.1' src='test.js'></script>", "test.js",
       "http://example.test/", ResourceType::kScript, 0},
      // Do not allow legacy languages in the "type" attribute.
      {"http://example.test",
       "<script type='javascript' src='test.js'></script>", nullptr,
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test",
       "<script type='javascript1.7' src='test.js'></script>", nullptr,
       "http://example.test/", ResourceType::kScript, 0},
      // Do not allow invalid types in the "type" attribute.
      {"http://example.test", "<script type='invalid' src='test.js'></script>",
       nullptr, "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test", "<script type='asdf' src='test.js'></script>",
       nullptr, "http://example.test/", ResourceType::kScript, 0},
      // Do not allow invalid languages.
      {"http://example.test",
       "<script language='french' src='test.js'></script>", nullptr,
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test",
       "<script language='python' src='test.js'></script>", nullptr,
       "http://example.test/", ResourceType::kScript, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

// Regression test for crbug.com/664744.
TEST_F(HTMLPreloadScannerTest, testUppercaseAsValues) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test", "<link rel=preload href=bla as=SCRIPT>", "bla",
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test", "<link rel=preload href=bla as=fOnT>", "bla",
       "http://example.test/", ResourceType::kFont, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, ReferrerHeader) {
  RunSetUp(kViewportEnabled, kPreloadEnabled,
           network::mojom::ReferrerPolicy::kAlways);

  KURL preload_url("http://example.test/sheet.css");
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |PageTestBase::dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      preload_url, "", WrappedResourceResponse(ResourceResponse()));

  ReferrerPolicyTestCase test_case = {
      "http://example.test",
      "<link rel='stylesheet' href='sheet.css' type='text/css'>",
      "sheet.css",
      "http://example.test/",
      ResourceType::kCSSStyleSheet,
      0,
      network::mojom::ReferrerPolicy::kAlways,
      "http://whatever.test/"};
  Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, Integrity) {
  IntegrityTestCase test_cases[] = {
      {0, "<script src=bla.js>"},
      {1,
       "<script src=bla.js "
       "integrity=sha256-qznLcsROx4GACP2dm0UCKCzCG+HiZ1guq6ZZDob/Tng=>"},
      {0, "<script src=bla.js integrity=sha257-XXX>"},
      {2,
       "<script src=bla.js "
       "integrity=sha256-qznLcsROx4GACP2dm0UCKCzCG+HiZ1guq6ZZDob/Tng= "
       "sha256-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxng=>"},
      {1,
       "<script src=bla.js "
       "integrity=sha256-qznLcsROx4GACP2dm0UCKCzCG+HiZ1guq6ZZDob/Tng= "
       "integrity=sha257-XXXX>"},
      {0,
       "<script src=bla.js integrity=sha257-XXX "
       "integrity=sha256-qznLcsROx4GACP2dm0UCKCzCG+HiZ1guq6ZZDob/Tng=>"},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

// Regression test for http://crbug.com/898795 where preloads after a
// dynamically inserted meta csp tag are dispatched on subsequent calls to the
// HTMLPreloadScanner, after they had been parsed.
TEST_F(HTMLPreloadScannerTest, MetaCsp_NoPreloadsAfter) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<meta http-equiv='Content-Security-Policy'><link rel=preload href=bla "
       "as=SCRIPT>",
       nullptr, "http://example.test/", ResourceType::kScript, 0},
      // The buffered text referring to the preload above should be cleared, so
      // make sure it is not preloaded on subsequent calls to Scan.
      {"http://example.test", "", nullptr, "http://example.test/",
       ResourceType::kScript, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, LazyLoadImage) {
  RunSetUp(kViewportEnabled);
  LazyLoadImageTestCase test_cases[] = {
      {"<img src='foo.jpg' loading='auto'>", true},
      {"<img src='foo.jpg' loading='lazy'>", false},
      {"<img src='foo.jpg' loading='eager'>", true},
  };
  for (const auto& test_case : test_cases)
    Test(test_case);
}

// https://crbug.com/1087854
TEST_F(HTMLPreloadScannerTest, CSSImportWithSemicolonInUrl) {
  PreloadScannerTestCase test_cases[] = {
      {"https://example.test",
       "<style>@import "
       "url(\"https://example2.test/css?foo=a;b&bar=d\");</style>",
       "https://example2.test/css?foo=a;b&bar=d", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       "<style>@import "
       "url('https://example2.test/css?foo=a;b&bar=d');</style>",
       "https://example2.test/css?foo=a;b&bar=d", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       "<style>@import "
       "url(https://example2.test/css?foo=a;b&bar=d);</style>",
       "https://example2.test/css?foo=a;b&bar=d", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       "<style>@import \"https://example2.test/css?foo=a;b&bar=d\";</style>",
       "https://example2.test/css?foo=a;b&bar=d", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       "<style>@import 'https://example2.test/css?foo=a;b&bar=d';</style>",
       "https://example2.test/css?foo=a;b&bar=d", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
  };

  for (const auto& test : test_cases)
    Test(test);
}

// https://crbug.com/1181291
TEST_F(HTMLPreloadScannerTest, TemplateInteractions) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test", "<template><img src='bla.gif'></template>",
       nullptr, "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<template><template><img src='bla.gif'></template></template>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<template><template></template><img src='bla.gif'></template>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<template><template></template><script "
       "src='test.js'></script></template>",
       nullptr, "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test",
       "<template><template></template><link rel=preload as=fetch "
       "href=bla></template>",
       nullptr, "http://example.test/", ResourceType::kRaw, 0},
      {"http://example.test",
       "<template><template></template><link rel='stylesheet' href='sheet.css' "
       "type='text/css'></template>",
       nullptr, "http://example.test/", ResourceType::kCSSStyleSheet, 0},
  };
  for (const auto& test : test_cases)
    Test(test);
}

// Regression test for https://crbug.com/1181291
TEST_F(HTMLPreloadScannerTest, JavascriptBaseUrl) {
  PreloadScannerTestCase test_cases[] = {
      {"",
       "<base href='javascript:'><base href='javascript:notallowed'><base "
       "href='http://example.test/'><link rel=preload href=bla as=SCRIPT>",
       "bla", "http://example.test/", ResourceType::kScript, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, OtherRulesBeforeImport) {
  PreloadScannerTestCase test_cases[] = {
      {"https://example.test",
       R"HTML(
       <style>
         @charset "utf-8";
         @import url("https://example2.test/lib.css");
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
       <style>
         @layer foo, bar;
         @import url("https://example2.test/lib.css");
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
       <style>
         @charset "utf-8";
         @layer foo, bar;
         @import url("https://example2.test/lib.css");
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
  };

  for (const auto& test : test_cases)
    Test(test);
}

TEST_F(HTMLPreloadScannerTest, PreloadLayeredImport) {
  PreloadScannerTestCase test_cases[] = {
      {"https://example.test",
       R"HTML(
       <style>
         @import url("https://example2.test/lib.css") layer
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
        <style>
          @import url("https://example2.test/lib.css") layer;
        </style>
        )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
        <style>
          @import url("https://example2.test/lib.css") layer(foo)
        </style>
        )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
        <style>
          @import url("https://example2.test/lib.css") layer(foo);
        </style>
        )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
       <style>
         @layer foo, bar;
         @import url("https://example2.test/lib.css") layer
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
       <style>
         @layer foo, bar;
         @import url("https://example2.test/lib.css") layer;
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
       <style>
         @layer foo, bar;
         @import url("https://example2.test/lib.css") layer(foo)
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
       <style>
         @layer foo, bar;
         @import url("https://example2.test/lib.css") layer(foo);
       </style>
       )HTML",
       "https://example2.test/lib.css", "https://example.test/",
       ResourceType::kCSSStyleSheet, 0},
      {"https://example.test",
       R"HTML(
        <style>
          @import url("https://example2.test/lib.css") layer foo;
        </style>
        )HTML",
       nullptr},
      {"https://example.test",
       R"HTML(
        <style>
          @import url("https://example2.test/lib.css") layer(foo) bar;
        </style>
        )HTML",
       nullptr},
      {"https://example.test",
       R"HTML(
        <style>
          @import url("https://example2.test/lib.css") layer();
        </style>
        )HTML",
       nullptr},
  };

  for (const auto& test : test_cases)
    Test(test);
}

TEST_F(HTMLPreloadScannerTest, TokenStreamMatcher) {
  ElementLocator locator;
  auto* c = locator.add_components()->mutable_id();
  c->set_id_attr("target");

  TokenStreamMatcherTestCase test_case = {locator,
                                          R"HTML(
    <div>
      <img src="not-interesting.jpg">
      <img src="super-interesting.jpg" id="target">
      <img src="not-interesting2.jpg">
    </div>
    )HTML",
                                          "super-interesting.jpg", true};
  Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testSharedStorageWritable) {
  WebRuntimeFeaturesBase::EnableSharedStorageAPI(true);
  WebRuntimeFeaturesBase::EnableSharedStorageAPIM118(true);
  static constexpr bool kSecureDocumentUrl = true;
  static constexpr bool kInsecureDocumentUrl = false;

  static constexpr char kSecureBaseURL[] = "https://example.test";
  static constexpr char kInsecureBaseURL[] = "http://example.test";

  SharedStorageWritableTestCase test_cases[] = {
      // Insecure context
      {kInsecureDocumentUrl, kSecureBaseURL,
       "<img src='/image' sharedstoragewritable>",
       /*expected_shared_storage_writable_opted_in=*/false},
      // No sharedstoragewritable attribute
      {kSecureDocumentUrl, kSecureBaseURL, "<img src='/image'>",
       /*expected_shared_storage_writable_opted_in=*/false},
      // Irrelevant element type
      {kSecureDocumentUrl, kSecureBaseURL,
       "<video poster='/image' sharedstoragewritable>",
       /*expected_shared_storage_writable_opted_in=*/false},
      // Secure context, sharedstoragewritable attribute
      // Base (initial) URL does not affect SharedStorageWritable eligibility
      {kSecureDocumentUrl, kInsecureBaseURL,
       "<img src='/image' sharedstoragewritable>",
       /*expected_shared_storage_writable_opted_in=*/true},
      // Secure context, sharedstoragewritable attribute
      {kSecureDocumentUrl, kSecureBaseURL,
       "<img src='/image' sharedstoragewritable>",
       /*expected_shared_storage_writable_opted_in=*/true},
  };

  for (const auto& test_case : test_cases) {
    RunSetUp(kViewportDisabled, kPreloadEnabled,
             network::mojom::ReferrerPolicy::kDefault,
             /*use_secure_document_url=*/test_case.use_secure_document_url);
    Test(test_case);
  }
}

enum class LcppPreloadLazyLoadImageType {
  kNativeLazyLoad,
  kCustomLazyLoad,
  kAll,
};

class HTMLPreloadScannerLCPPLazyLoadImageTest
    : public HTMLPreloadScannerTest,
      public testing::WithParamInterface<LcppPreloadLazyLoadImageType> {
 public:
  HTMLPreloadScannerLCPPLazyLoadImageTest() {
    switch (GetParam()) {
      case LcppPreloadLazyLoadImageType::kNativeLazyLoad:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            blink::features::kLCPPLazyLoadImagePreload,
            {{blink::features::kLCPCriticalPathPredictorPreloadLazyLoadImageType
                  .name,
              "native_lazy_loading"}});
        break;
      case LcppPreloadLazyLoadImageType::kCustomLazyLoad:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            blink::features::kLCPPLazyLoadImagePreload,
            {{blink::features::kLCPCriticalPathPredictorPreloadLazyLoadImageType
                  .name,
              "custom_lazy_loading"}});
        break;
      case LcppPreloadLazyLoadImageType::kAll:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            blink::features::kLCPPLazyLoadImagePreload,
            {{blink::features::kLCPCriticalPathPredictorPreloadLazyLoadImageType
                  .name,
              "all"}});
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HTMLPreloadScannerLCPPLazyLoadImageTest,
    ::testing::Values(LcppPreloadLazyLoadImageType::kNativeLazyLoad,
                      LcppPreloadLazyLoadImageType::kCustomLazyLoad,
                      LcppPreloadLazyLoadImageType::kAll));

TEST_P(HTMLPreloadScannerLCPPLazyLoadImageTest,
       TokenStreamMatcherWithLoadingLazy) {
  ElementLocator locator;
  auto* c = locator.add_components()->mutable_id();
  c->set_id_attr("target");

  switch (GetParam()) {
    case LcppPreloadLazyLoadImageType::kNativeLazyLoad:
      CachedDocumentParameters::SetLcppPreloadLazyLoadImageTypeForTesting(
          features::LcppPreloadLazyLoadImageType::kNativeLazyLoading);
      Test(TokenStreamMatcherTestCase{locator, R"HTML(
        <div>
          <img src="not-interesting.jpg">
          <img src="super-interesting.jpg" id="target" loading="lazy">
          <img src="not-interesting2.jpg">
        </div>
        )HTML",
                                      "super-interesting.jpg", true});
      break;
    case LcppPreloadLazyLoadImageType::kCustomLazyLoad:
      CachedDocumentParameters::SetLcppPreloadLazyLoadImageTypeForTesting(
          features::LcppPreloadLazyLoadImageType::kCustomLazyLoading);
      Test(TokenStreamMatcherTestCase{locator, R"HTML(
        <div>
          <img src="not-interesting.jpg">
          <img data-src="super-interesting.jpg" id="target">
          <img src="not-interesting2.jpg">
        </div>
        )HTML",
                                      "super-interesting.jpg", true});
      break;
    case LcppPreloadLazyLoadImageType::kAll:
      CachedDocumentParameters::SetLcppPreloadLazyLoadImageTypeForTesting(
          features::LcppPreloadLazyLoadImageType::kAll);
      Test(TokenStreamMatcherTestCase{locator, R"HTML(
        <div>
          <img src="not-interesting.jpg">
          <img src="super-interesting.jpg" id="target" loading="lazy">
          <img src="not-interesting2.jpg">
        </div>
        )HTML",
                                      "super-interesting.jpg", true});
      Test(TokenStreamMatcherTestCase{locator, R"HTML(
        <div>
          <img src="not-interesting.jpg">
          <img data-src="super-interesting.jpg" id="target">
          <img src="not-interesting2.jpg">
        </div>
        )HTML",
                                      "super-interesting.jpg", true});
      break;
  }

  CachedDocumentParameters::SetLcppPreloadLazyLoadImageTypeForTesting(
      std::nullopt);
}

TEST_P(HTMLPreloadScannerLCPPLazyLoadImageTest,
       TokenStreamMatcherWithLoadingLazyAutoSizes) {
  ElementLocator locator;
  auto* c = locator.add_components()->mutable_id();
  c->set_id_attr("target");

  switch (GetParam()) {
    case LcppPreloadLazyLoadImageType::kNativeLazyLoad:
    case LcppPreloadLazyLoadImageType::kCustomLazyLoad:
    case LcppPreloadLazyLoadImageType::kAll:
      Test(TokenStreamMatcherTestCase{locator, R"HTML(
        <div>
          <img src="not-interesting.jpg">
          <img src="super-interesting.jpg" id="target" loading="lazy" sizes="auto">
          <img src="not-interesting2.jpg">
        </div>
        )HTML",
                                      nullptr, false});
      break;
  }
}

TEST_F(HTMLPreloadScannerTest, PreloadScanDisabled_NoPreloads) {
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test", "<img src='bla.gif'>", /* preloaded_url=*/nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test", "<script src='test.js'></script>",
       /* preloaded_url=*/nullptr, "http://example.test/",
       ResourceType::kScript, 0}};

  for (const auto& test_case : test_cases) {
    RunSetUp(kViewportDisabled, kPreloadEnabled,
             network::mojom::ReferrerPolicy::kDefault, true, {},
             /* disable_preload_scanning=*/true);
    Test(test_case);
  }
}

}  // namespace blink
