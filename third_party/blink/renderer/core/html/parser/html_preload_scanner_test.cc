// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"

#include <memory>
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_resource_preloader.h"
#include "third_party/blink/renderer/core/html/parser/preload_request.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
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
  bool lazy_load_image_enabled;
};

class HTMLMockHTMLResourcePreloader : public ResourcePreloader {
 public:
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
      EXPECT_EQ(width, preload_request_->ResourceWidth());
      EXPECT_EQ(preferences.ShouldSend(mojom::WebClientHintsType::kDpr),
                preload_request_->Preferences().ShouldSend(
                    mojom::WebClientHintsType::kDpr));
      EXPECT_EQ(
          preferences.ShouldSend(mojom::WebClientHintsType::kResourceWidth),
          preload_request_->Preferences().ShouldSend(
              mojom::WebClientHintsType::kResourceWidth));
      EXPECT_EQ(
          preferences.ShouldSend(mojom::WebClientHintsType::kViewportWidth),
          preload_request_->Preferences().ShouldSend(
              mojom::WebClientHintsType::kViewportWidth));
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
    EXPECT_EQ(expected_referrer, resource->GetResourceRequest().HttpReferrer());
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
      EXPECT_TRUE(preload_request_->Nonce().IsEmpty());
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

  void LazyLoadImageEnabledVerification(bool expected_enabled) {
    ASSERT_TRUE(preload_request_.get());
    EXPECT_EQ(expected_enabled,
              preload_request_->IsLazyLoadImageEnabledForTesting());
  }

 protected:
  void Preload(std::unique_ptr<PreloadRequest> preload_request) override {
    preload_request_ = std::move(preload_request);
  }

 private:
  std::unique_ptr<PreloadRequest> preload_request_;
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

  MediaValuesCached::MediaValuesCachedData CreateMediaValuesData() {
    MediaValuesCached::MediaValuesCachedData data;
    data.viewport_width = 500;
    data.viewport_height = 600;
    data.device_width = 700;
    data.device_height = 800;
    data.device_pixel_ratio = 2.0;
    data.color_bits_per_component = 24;
    data.monochrome_bits_per_component = 0;
    data.primary_pointer_type = kPointerTypeFine;
    data.default_font_size = 16;
    data.three_d_enabled = true;
    data.media_type = media_type_names::kScreen;
    data.strict_mode = true;
    data.display_mode = blink::mojom::DisplayMode::kBrowser;
    return data;
  }

  void RunSetUp(ViewportState viewport_state,
                PreloadState preload_state = kPreloadEnabled,
                network::mojom::ReferrerPolicy document_referrer_policy =
                    network::mojom::ReferrerPolicy::kDefault,
                bool use_secure_document_url = false) {
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
    GetDocument().SetReferrerPolicy(document_referrer_policy);
    scanner_ = std::make_unique<HTMLPreloadScanner>(
        options, document_url,
        std::make_unique<CachedDocumentParameters>(&GetDocument()),
        CreateMediaValuesData(),
        TokenPreloadScanner::ScannerType::kMainDocument);
  }

  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    RunSetUp(kViewportEnabled);
  }

  void Test(PreloadScannerTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);

    preloader.PreloadRequestVerification(
        test_case.type, test_case.preloaded_url, test_case.output_base_url,
        test_case.resource_width, test_case.preferences);
  }

  void Test(HTMLPreconnectTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);
    preloader.PreconnectRequestVerification(test_case.preconnected_host,
                                            test_case.cross_origin);
  }

  void Test(ReferrerPolicyTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);

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
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);
    preloader.CorsRequestVerification(&GetDocument(), test_case.request_mode,
                                      test_case.credentials_mode);
  }

  void Test(CSPTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url(test_case.base_url);
    seen_csp_meta_tag_ = false;
    scanner_->AppendToEnd(String(test_case.input_html));
    scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    EXPECT_EQ(test_case.should_see_csp_tag, seen_csp_meta_tag_);
  }

  void Test(NonceTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);
    preloader.NonceRequestVerification(test_case.nonce);
  }

  void Test(ContextTestCase test_case) {
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url(test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);

    preloader.ContextVerification(test_case.is_image_set);
  }

  void Test(IntegrityTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url("http://example.test/");
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);

    preloader.CheckNumberOfIntegrityConstraints(
        test_case.number_of_integrity_metadata_found);
  }

  void Test(LazyLoadImageTestCase test_case) {
    SCOPED_TRACE(test_case.input_html);
    HTMLMockHTMLResourcePreloader preloader;
    KURL base_url("http://example.test/");
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests =
        scanner_->Scan(base_url, nullptr, seen_csp_meta_tag_);
    preloader.TakeAndPreload(requests);
    preloader.LazyLoadImageEnabledVerification(
        test_case.lazy_load_image_enabled);
  }

 private:
  std::unique_ptr<HTMLPreloadScanner> scanner_;
  bool seen_csp_meta_tag_ = false;
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
  ClientHintsPreferences dpr;
  ClientHintsPreferences resource_width;
  ClientHintsPreferences all;
  ClientHintsPreferences viewport_width;
  dpr.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  all.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  resource_width.SetShouldSendForTesting(
      mojom::WebClientHintsType::kResourceWidth);
  all.SetShouldSendForTesting(mojom::WebClientHintsType::kResourceWidth);
  viewport_width.SetShouldSendForTesting(
      mojom::WebClientHintsType::kViewportWidth);
  all.SetShouldSendForTesting(mojom::WebClientHintsType::kViewportWidth);
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
       "<meta http-equiv='accept-ch' content='dpr \t'><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0, dpr},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='bla,dpr \t'><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0, dpr},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  width  '><img sizes='100vw' "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 500,
       resource_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  width  , wutever'><img "
       "sizes='300px' srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 300,
       resource_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  '><img "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       viewport_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  , "
       "wutever'><img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", ResourceType::kImage, 0,
       viewport_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  ,width, "
       "wutever, dpr \t'><img sizes='90vw' srcset='bla.gif 320w, blabla.gif "
       "640w'>",
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
  all.SetShouldSendForTesting(mojom::WebClientHintsType::kDpr);
  all.SetShouldSendForTesting(mojom::WebClientHintsType::kResourceWidth);
  all.SetShouldSendForTesting(mojom::WebClientHintsType::kViewportWidth);

  const PreloadScannerTestCase expect_no_client_hint = {
      "http://example.test",
      "<meta http-equiv='accept-ch' content='  viewport-width  ,width, "
      "wutever, dpr \t'><img sizes='90vw' srcset='bla.gif 320w, blabla.gif "
      "640w'>",
      "blabla.gif",
      "http://example.test/",
      ResourceType::kImage,
      450};

  const PreloadScannerTestCase expect_client_hint = {
      "http://example.test",
      "<meta http-equiv='accept-ch' content='  viewport-width  ,width, "
      "wutever, dpr \t'><img sizes='90vw' srcset='bla.gif 320w, blabla.gif "
      "640w'>",
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
       network::mojom::ReferrerPolicy::
           kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
       nullptr},
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
      {"http://example.test", "<link rel='import' href='/import'>", ""},
      {"http://example.test", "<link rel='import' href='/import' nonce=''>",
       ""},
      {"http://example.test", "<link rel='import' href='/import' nonce='abc'>",
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
      // Allow legacy languages in the "type" attribute.
      {"http://example.test",
       "<script type='javascript' src='test.js'></script>", "test.js",
       "http://example.test/", ResourceType::kScript, 0},
      {"http://example.test",
       "<script type='javascript1.7' src='test.js'></script>", "test.js",
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

TEST_F(HTMLPreloadScannerTest, LazyLoadImage_DisabledForSmallImages) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(true);
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test(true);
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(
          false);
  GetDocument().GetSettings()->SetLazyLoadEnabled(true);
  RunSetUp(kViewportEnabled);
  LazyLoadImageTestCase test_cases[] = {
      {"<img src='foo.jpg'>", true},
      {"<img src='foo.jpg' height='1px' width='1px'>", false},
      {"<img src='foo.jpg' style='height: 1px; width: 1px'>", false},
      {"<img src='foo.jpg' height='10px' width='10px'>", false},
      {"<img src='foo.jpg' style='height: 10px; width: 10px'>", false},
      {"<img src='foo.jpg' height='1px'>", true},
      {"<img src='foo.jpg' style='height: 1px;'>", true},
      {"<img src='foo.jpg' width='1px'>", true},
      {"<img src='foo.jpg' style='width: 1px;'>", true},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, LazyLoadImage_FeatureDisabledWithAttribute) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(false);
  GetDocument().GetSettings()->SetLazyLoadEnabled(true);
  RunSetUp(kViewportEnabled);
  LazyLoadImageTestCase test_cases[] = {
      {"<img src='foo.jpg' loading='auto'>", false},
      {"<img src='foo.jpg' loading='lazy'>", false},
      {"<img src='foo.jpg' loading='eager'>", false},
  };
  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest,
       LazyLoadImage_FeatureAutomaticEnabledWithAttribute) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(true);
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test(true);
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(
          false);
  GetDocument().GetSettings()->SetLazyLoadEnabled(true);
  RunSetUp(kViewportEnabled);
  LazyLoadImageTestCase test_cases[] = {
      {"<img src='foo.jpg' loading='auto'>", true},
      {"<img src='foo.jpg' loading='lazy'>", true},
      {"<img src='foo.jpg' loading='eager'>", false},
      // loading=lazy should override other conditions.
      {"<img src='foo.jpg' style='height: 1px;' loading='lazy'>", true},
      {"<img src='foo.jpg' style='height: 1px; width: 1px' loading='lazy'>",
       true},
  };
  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest,
       LazyLoadImage_FeatureExplicitEnabledWithAttribute) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test(true);
  GetDocument().GetSettings()->SetLazyLoadEnabled(true);
  RunSetUp(kViewportEnabled);
  LazyLoadImageTestCase test_cases[] = {
      {"<img src='foo.jpg' loading='auto'>", false},
      {"<img src='foo.jpg' loading='lazy'>", true},
      {"<img src='foo.jpg' loading='eager'>", false},
  };
  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest,
       LazyLoadImage_FeatureAutomaticPreloadForLargeImages) {
  // Large images should not be preloaded, when loading is auto or lazy.
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(true);
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test(true);
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(
          false);
  GetDocument().GetSettings()->SetLazyLoadEnabled(true);
  RunSetUp(kViewportEnabled);
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test", "<img src='foo.jpg' height='20px' width='20px'>",
       nullptr, "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<img src='foo.jpg' style='height: 20px; width: 20px'>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<img src='foo.jpg' height='20px' width='20px' loading='lazy'>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<img src='foo.jpg' height='20px' width='20px' loading='auto'>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<img src='foo.jpg' style='height: 20px; width: 20px' loading='lazy'>",
       nullptr, "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<img src='foo.jpg' style='height: 20px; width: 20px' loading='auto'>",
       nullptr, "http://example.test/", ResourceType::kImage, 0},
  };
  for (const auto& test_case : test_cases)
    Test(test_case);

  // When loading is eager, lazyload is disabled and preload happens.
  LazyLoadImageTestCase test_cases_that_preload[] = {
      {"<img src='foo.jpg' height='20px' width='20px' loading='eager'>", false},
      {"<img src='foo.jpg' style='height: 20px; width: 20px' loading='eager'>",
       false},
  };
  for (const auto& test_case : test_cases_that_preload)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest,
       LazyLoadImage_FeatureExplicitPreloadForLargeImages) {
  // Large images should not be preloaded, when loading is lazy.
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedLazyImageLoadingMetadataFetchForTest
      scoped_lazy_image_loading_metadata_fetch_for_test(true);
  GetDocument().GetSettings()->SetLazyLoadEnabled(true);
  RunSetUp(kViewportEnabled);
  PreloadScannerTestCase test_cases[] = {
      {"http://example.test",
       "<img src='foo.jpg' height='20px' width='20px' loading='lazy'>", nullptr,
       "http://example.test/", ResourceType::kImage, 0},
      {"http://example.test",
       "<img src='foo.jpg' style='height: 20px; width: 20px' loading='lazy'>",
       nullptr, "http://example.test/", ResourceType::kImage, 0},
  };
  for (const auto& test_case : test_cases)
    Test(test_case);

  // When loading is eager or auto, lazyload is disabled and preload happens.
  LazyLoadImageTestCase test_cases_that_preload[] = {
      {"<img src='foo.jpg' height='20px' width='20px' loading='eager'>", false},
      {"<img src='foo.jpg' style='height: 20px; width: 20px' loading='eager'>",
       false},
      {"<img src='foo.jpg' height='20px' width='20px' loading='auto'>", false},
      {"<img src='foo.jpg' style='height: 20px; width: 20px' loading='auto'>",
       false},
  };
  for (const auto& test_case : test_cases_that_preload)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, LazyLoadImage_DisableMetadataFetch) {
  GetDocument().GetSettings()->SetLazyLoadEnabled(true);
  struct TestCase {
    bool metadata_fetch_feature_enabled;
    bool automatic_lazy_image_loading_enabled;
    const char* loading_attr_value;
    bool expected_is_preload;
    // If preload happens, whether it is a fetch of placeholder or full image.
    bool expected_is_placeholder_fetch;
  };
  const TestCase test_cases[] = {
      // The lazyload eligible cases should not trigger any preload when
      // metadata fetch feature disabled, and trigger placeholder fetch if
      // metadata fetch feature is active.
      {false, false, "lazy", false, false},
      {false, true, "lazy", false, false},
      {false, true, "auto", false, false},
      {true, false, "lazy", true, true},
      {true, true, "lazy", true, true},
      {true, true, "auto", true, true},

      // Lazyload ineligible case.
      {false, false, "auto", true, false},
      {true, false, "auto", true, false},

      // Full image should be fetched when loading='eager' irrespective of
      // automatic lazyload or metadata fetch feature states.
      {false, false, "eager", true, false},
      {false, true, "eager", true, false},
      {true, false, "eager", true, false},
      {true, true, "eager", true, false},
  };
  for (const auto& test_case : test_cases) {
    ScopedLazyImageLoadingMetadataFetchForTest
        scoped_lazy_image_loading_metadata_fetch_for_test(
            test_case.metadata_fetch_feature_enabled);
    ScopedAutomaticLazyImageLoadingForTest
        scoped_automatic_lazy_image_loading_for_test(
            test_case.automatic_lazy_image_loading_enabled);
    ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
        scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(
            false);
    RunSetUp(kViewportEnabled);
    const std::string img_html = base::StringPrintf(
        "<img src='foo.jpg' loading='%s'>", test_case.loading_attr_value);
    if (test_case.expected_is_preload) {
      LazyLoadImageTestCase test_preload = {
          img_html.c_str(), test_case.expected_is_placeholder_fetch};
      Test(test_preload);
    } else {
      PreloadScannerTestCase test_no_preload = {
          "http://example.test",  img_html.c_str(),     nullptr,
          "http://example.test/", ResourceType::kImage, 0};
      Test(test_no_preload);
    }
  }
}

}  // namespace blink
