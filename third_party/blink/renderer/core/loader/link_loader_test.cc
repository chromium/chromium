// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/link_loader.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/link_loader_client.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/pending_link_preload.h"
#include "third_party/blink/renderer/core/loader/resource/link_dictionary_resource.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class MockLinkLoaderClient final
    : public GarbageCollected<MockLinkLoaderClient>,
      public LinkLoaderClient {
 public:
  explicit MockLinkLoaderClient(bool should_load) : should_load_(should_load) {}

  void Trace(Visitor* visitor) const override {
    LinkLoaderClient::Trace(visitor);
  }

  bool ShouldLoadLink() override { return should_load_; }
  bool IsLinkCreatedByParser() override { return true; }

  void LinkLoaded() override {}
  void LinkLoadingErrored() override {}

 private:
  const bool should_load_;
};

class NetworkHintsMock : public WebPrescientNetworking {
 public:
  NetworkHintsMock() = default;

  void PrefetchDNS(const WebURL& url) override { did_dns_prefetch_ = true; }
  void Preconnect(const WebURL& url, bool allow_credentials) override {
    did_preconnect_ = true;
    is_https_ = url.ProtocolIs("https");
    allow_credentials_ = allow_credentials;
  }

  bool DidDnsPrefetch() { return did_dns_prefetch_; }
  bool DidPreconnect() { return did_preconnect_; }
  bool IsHTTPS() { return is_https_; }
  bool AllowCredentials() { return allow_credentials_; }

 private:
  mutable bool did_dns_prefetch_ = false;
  mutable bool did_preconnect_ = false;
  mutable bool is_https_ = false;
  mutable bool allow_credentials_ = false;
};

class LinkLoaderPreloadTestBase : public testing::Test,
                                  private ScopedMockOverlayScrollbars {
 public:
  struct Expectations {
    ResourceLoadPriority priority;
    mojom::blink::RequestContextType context;
    bool link_loader_should_load_value;
    KURL load_url;
    network::mojom::ReferrerPolicy referrer_policy;
  };

  LinkLoaderPreloadTestBase() {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
  }

  ~LinkLoaderPreloadTestBase() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void TestPreload(const LinkLoadParameters& params,
                   const Expectations& expected) {
    ResourceFetcher* fetcher = dummy_page_holder_->GetDocument().Fetcher();
    ASSERT_TRUE(fetcher);
    dummy_page_holder_->GetFrame().GetSettings()->SetScriptEnabled(true);
    Persistent<MockLinkLoaderClient> loader_client =
        MakeGarbageCollected<MockLinkLoaderClient>(
            expected.link_loader_should_load_value);
    auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |dummy_page_holder_|.
    url_test_helpers::RegisterMockedErrorURLLoad(params.href);
    loader->LoadLink(params, dummy_page_holder_->GetDocument());
    if (!expected.load_url.IsNull() &&
        expected.priority != ResourceLoadPriority::kUnresolved) {
      ASSERT_EQ(1, fetcher->CountPreloads());
      Resource* resource = loader->GetResourceForTesting();
      ASSERT_NE(resource, nullptr);
      EXPECT_EQ(expected.load_url.GetString(), resource->Url().GetString());
      EXPECT_TRUE(fetcher->ContainsAsPreload(resource));
      EXPECT_EQ(expected.priority, resource->GetResourceRequest().Priority());
      EXPECT_EQ(expected.context,
                resource->GetResourceRequest().GetRequestContext());
      if (expected.referrer_policy !=
          network::mojom::ReferrerPolicy::kDefault) {
        EXPECT_EQ(expected.referrer_policy,
                  resource->GetResourceRequest().GetReferrerPolicy());
      }
    } else {
      ASSERT_EQ(0, fetcher->CountPreloads());
    }
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

struct PreloadTestParams {
  const char* href;
  const char* as;
  const ResourceLoadPriority priority;
  const mojom::blink::RequestContextType context;
  const bool expecting_load;
};

constexpr PreloadTestParams kPreloadTestParams[] = {
    {"http://example.test/cat.jpg", "image", ResourceLoadPriority::kLow,
     mojom::blink::RequestContextType::IMAGE, true},
    {"http://example.test/cat.js", "script", ResourceLoadPriority::kHigh,
     mojom::blink::RequestContextType::SCRIPT, true},
    {"http://example.test/cat.css", "style", ResourceLoadPriority::kVeryHigh,
     mojom::blink::RequestContextType::STYLE, true},
    // TODO(yoav): It doesn't seem like the audio context is ever used. That
    // should probably be fixed (or we can consolidate audio and video).
    //
    // Until the preload cache is defined in terms of range requests and media
    // fetches we can't reliably preload audio/video content and expect it to be
    // served from the cache correctly. Until
    // https://github.com/w3c/preload/issues/97 is resolved and implemented we
    // need to disable these preloads.
    {"http://example.test/cat.wav", "audio", ResourceLoadPriority::kLow,
     mojom::blink::RequestContextType::AUDIO, false},
    {"http://example.test/cat.mp4", "video", ResourceLoadPriority::kLow,
     mojom::blink::RequestContextType::VIDEO, false},
    {"http://example.test/cat.vtt", "track", ResourceLoadPriority::kLow,
     mojom::blink::RequestContextType::TRACK, true},
    {"http://example.test/cat.woff", "font", ResourceLoadPriority::kHigh,
     mojom::blink::RequestContextType::FONT, true},
    // TODO(yoav): subresource should be *very* low priority (rather than
    // low).
    {"http://example.test/cat.empty", "fetch", ResourceLoadPriority::kHigh,
     mojom::blink::RequestContextType::SUBRESOURCE, true},
    {"http://example.test/cat.blob", "blabla", ResourceLoadPriority::kLow,
     mojom::blink::RequestContextType::SUBRESOURCE, false},
    {"http://example.test/cat.blob", "", ResourceLoadPriority::kLow,
     mojom::blink::RequestContextType::SUBRESOURCE, false},
    {"bla://example.test/cat.gif", "image", ResourceLoadPriority::kUnresolved,
     mojom::blink::RequestContextType::IMAGE, false}};

class LinkLoaderPreloadTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadTestParams> {};

TEST_P(LinkLoaderPreloadTest, Preload) {
  const auto& test_case = GetParam();
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, String(),
      test_case.as, String(), String(), String(), String(),
      network::mojom::ReferrerPolicy::kDefault, KURL(NullURL(), test_case.href),
      String(), String(), String());
  Expectations expectations = {
      test_case.priority, test_case.context, test_case.expecting_load,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadTest,
                         LinkLoaderPreloadTest,
                         testing::ValuesIn(kPreloadTestParams));

struct PreloadMimeTypeTestParams {
  const char* href;
  const char* as;
  const char* type;
  const ResourceLoadPriority priority;
  const mojom::blink::RequestContextType context;
  const bool expecting_load;
};

constexpr PreloadMimeTypeTestParams kPreloadMimeTypeTestParams[] = {
    {"http://example.test/cat.webp", "image", "image/webp",
     ResourceLoadPriority::kLow, mojom::blink::RequestContextType::IMAGE, true},
    {"http://example.test/cat.svg", "image", "image/svg+xml",
     ResourceLoadPriority::kLow, mojom::blink::RequestContextType::IMAGE, true},
    {"http://example.test/cat.jxr", "image", "image/jxr",
     ResourceLoadPriority::kUnresolved, mojom::blink::RequestContextType::IMAGE,
     false},
    {"http://example.test/cat.js", "script", "text/javascript",
     ResourceLoadPriority::kHigh, mojom::blink::RequestContextType::SCRIPT,
     true},
    {"http://example.test/cat.js", "script", "text/coffeescript",
     ResourceLoadPriority::kUnresolved,
     mojom::blink::RequestContextType::SCRIPT, false},
    {"http://example.test/cat.css", "style", "text/css",
     ResourceLoadPriority::kVeryHigh, mojom::blink::RequestContextType::STYLE,
     true},
    {"http://example.test/cat.css", "style", "text/sass",
     ResourceLoadPriority::kUnresolved, mojom::blink::RequestContextType::STYLE,
     false},
    // Until the preload cache is defined in terms of range requests and media
    // fetches we can't reliably preload audio/video content and expect it to be
    // served from the cache correctly. Until
    // https://github.com/w3c/preload/issues/97 is resolved and implemented we
    // need to disable these preloads.
    {"http://example.test/cat.wav", "audio", "audio/wav",
     ResourceLoadPriority::kLow, mojom::blink::RequestContextType::AUDIO,
     false},
    {"http://example.test/cat.wav", "audio", "audio/mp57",
     ResourceLoadPriority::kUnresolved, mojom::blink::RequestContextType::AUDIO,
     false},
    {"http://example.test/cat.webm", "video", "video/webm",
     ResourceLoadPriority::kLow, mojom::blink::RequestContextType::VIDEO,
     false},
    {"http://example.test/cat.mp199", "video", "video/mp199",
     ResourceLoadPriority::kUnresolved, mojom::blink::RequestContextType::VIDEO,
     false},
    {"http://example.test/cat.vtt", "track", "text/vtt",
     ResourceLoadPriority::kLow, mojom::blink::RequestContextType::TRACK, true},
    {"http://example.test/cat.vtt", "track", "text/subtitlething",
     ResourceLoadPriority::kUnresolved, mojom::blink::RequestContextType::TRACK,
     false},
    {"http://example.test/cat.woff", "font", "font/woff2",
     ResourceLoadPriority::kHigh, mojom::blink::RequestContextType::FONT, true},
    {"http://example.test/cat.woff", "font", "font/woff84",
     ResourceLoadPriority::kUnresolved, mojom::blink::RequestContextType::FONT,
     false},
    {"http://example.test/cat.empty", "fetch", "foo/bar",
     ResourceLoadPriority::kHigh, mojom::blink::RequestContextType::SUBRESOURCE,
     true},
    {"http://example.test/cat.blob", "blabla", "foo/bar",
     ResourceLoadPriority::kLow, mojom::blink::RequestContextType::SUBRESOURCE,
     false},
    {"http://example.test/cat.blob", "", "foo/bar", ResourceLoadPriority::kLow,
     mojom::blink::RequestContextType::SUBRESOURCE, false}};

class LinkLoaderPreloadMimeTypeTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadMimeTypeTestParams> {};

TEST_P(LinkLoaderPreloadMimeTypeTest, Preload) {
  const auto& test_case = GetParam();
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, test_case.type,
      test_case.as, String(), String(), String(), String(),
      network::mojom::ReferrerPolicy::kDefault, KURL(NullURL(), test_case.href),
      String(), String(), String());
  Expectations expectations = {
      test_case.priority, test_case.context, test_case.expecting_load,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadMimeTypeTest,
                         LinkLoaderPreloadMimeTypeTest,
                         testing::ValuesIn(kPreloadMimeTypeTestParams));

struct PreloadMediaTestParams {
  const char* media;
  const ResourceLoadPriority priority;
  const bool link_loader_should_load_value;
  const bool expecting_load;
};

constexpr PreloadMediaTestParams kPreloadMediaTestParams[] = {
    {"(max-width: 600px)", ResourceLoadPriority::kLow, true, true},
    {"(max-width: 400px)", ResourceLoadPriority::kUnresolved, true, false},
    {"(max-width: 600px)", ResourceLoadPriority::kLow, false, false}};

class LinkLoaderPreloadMediaTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadMediaTestParams> {};

TEST_P(LinkLoaderPreloadMediaTest, Preload) {
  const auto& test_case = GetParam();
  LinkLoadParameters params(LinkRelAttribute("preload"),
                            kCrossOriginAttributeNotSet, "image/gif", "image",
                            test_case.media, String(), String(), String(),
                            network::mojom::ReferrerPolicy::kDefault,
                            KURL(NullURL(), "http://example.test/cat.gif"),
                            String(), String(), String());
  Expectations expectations = {
      test_case.priority, mojom::blink::RequestContextType::IMAGE,
      test_case.link_loader_should_load_value,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadMediaTest,
                         LinkLoaderPreloadMediaTest,
                         testing::ValuesIn(kPreloadMediaTestParams));

constexpr network::mojom::ReferrerPolicy kPreloadReferrerPolicyTestParams[] = {
    network::mojom::ReferrerPolicy::kOrigin,
    network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin,
    network::mojom::ReferrerPolicy::kSameOrigin,
    network::mojom::ReferrerPolicy::kStrictOrigin,
    network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
    network::mojom::ReferrerPolicy::kNever};

class LinkLoaderPreloadReferrerPolicyTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<network::mojom::ReferrerPolicy> {};

TEST_P(LinkLoaderPreloadReferrerPolicyTest, Preload) {
  const network::mojom::ReferrerPolicy referrer_policy = GetParam();
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, "image/gif",
      "image", String(), String(), String(), String(), referrer_policy,
      KURL(NullURL(), "http://example.test/cat.gif"), String(), String(),
      String());
  Expectations expectations = {ResourceLoadPriority::kLow,
                               mojom::blink::RequestContextType::IMAGE, true,
                               params.href, referrer_policy};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadReferrerPolicyTest,
                         LinkLoaderPreloadReferrerPolicyTest,
                         testing::ValuesIn(kPreloadReferrerPolicyTestParams));

struct PreloadNonceTestParams {
  const char* nonce;
  const char* content_security_policy;
  const bool expecting_load;
};

constexpr PreloadNonceTestParams kPreloadNonceTestParams[] = {
    {"abc", "script-src 'nonce-abc'", true},
    {"", "script-src 'nonce-abc'", false},
    {"def", "script-src 'nonce-abc'", false},
};

class LinkLoaderPreloadNonceTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadNonceTestParams> {};

TEST_P(LinkLoaderPreloadNonceTest, Preload) {
  const auto& test_case = GetParam();
  dummy_page_holder_->GetFrame()
      .DomWindow()
      ->GetContentSecurityPolicy()
      ->AddPolicies(ParseContentSecurityPolicies(
          test_case.content_security_policy,
          network::mojom::ContentSecurityPolicyType::kEnforce,
          network::mojom::ContentSecurityPolicySource::kHTTP,
          *(dummy_page_holder_->GetFrame().DomWindow()->GetSecurityOrigin())));
  LinkLoadParameters params(LinkRelAttribute("preload"),
                            kCrossOriginAttributeNotSet, String(), "script",
                            String(), test_case.nonce, String(), String(),
                            network::mojom::ReferrerPolicy::kDefault,
                            KURL(NullURL(), "http://example.test/cat.js"),
                            String(), String(), String());
  Expectations expectations = {
      ResourceLoadPriority::kHigh, mojom::blink::RequestContextType::SCRIPT,
      test_case.expecting_load,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadNonceTest,
                         LinkLoaderPreloadNonceTest,
                         testing::ValuesIn(kPreloadNonceTestParams));

struct PreloadImageSrcsetTestParams {
  const char* href;
  const char* image_srcset;
  const char* image_sizes;
  float scale_factor;
  const char* expected_url;
};

constexpr PreloadImageSrcsetTestParams kPreloadImageSrcsetTestParams[] = {
    {"http://example.test/cat.gif",
     "http://example.test/cat1x.gif 1x, http://example.test/cat2x.gif 2x",
     nullptr, 1.0, "http://example.test/cat1x.gif"},
    {"http://example.test/cat.gif",
     "http://example.test/cat1x.gif 1x, http://example.test/cat2x.gif 2x",
     nullptr, 2.0, "http://example.test/cat2x.gif"},
    {"http://example.test/cat.gif",
     "http://example.test/cat400.gif 400w, http://example.test/cat800.gif 800w",
     "400px", 1.0, "http://example.test/cat400.gif"},
    {"http://example.test/cat.gif",
     "http://example.test/cat400.gif 400w, http://example.test/cat800.gif 800w",
     "400px", 2.0, "http://example.test/cat800.gif"},
    {"http://example.test/cat.gif",
     "cat200.gif 200w, cat400.gif 400w, cat800.gif 800w", "200px", 1.0,
     "http://example.test/cat200.gif"},
};

class LinkLoaderPreloadImageSrcsetTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadImageSrcsetTestParams> {};

TEST_P(LinkLoaderPreloadImageSrcsetTest, Preload) {
  const auto& test_case = GetParam();
  dummy_page_holder_->GetDocument().SetBaseURLOverride(
      KURL("http://example.test/"));
  dummy_page_holder_->GetDocument().GetFrame()->SetLayoutZoomFactor(
      test_case.scale_factor);
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, "image/gif",
      "image", String(), String(), String(), String(),
      network::mojom::ReferrerPolicy::kDefault, KURL(NullURL(), test_case.href),
      test_case.image_srcset, test_case.image_sizes, String());
  Expectations expectations = {ResourceLoadPriority::kLow,
                               mojom::blink::RequestContextType::IMAGE, true,
                               KURL(NullURL(), test_case.expected_url),
                               network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadImageSrcsetTest,
                         LinkLoaderPreloadImageSrcsetTest,
                         testing::ValuesIn(kPreloadImageSrcsetTestParams));

struct ModulePreloadTestParams {
  const char* href;
  const char* nonce;
  const char* integrity;
  CrossOriginAttributeValue cross_origin;
  network::mojom::ReferrerPolicy referrer_policy;
  bool expecting_load;
  network::mojom::CredentialsMode expected_credentials_mode;
};

constexpr ModulePreloadTestParams kModulePreloadTestParams[] = {
    {"", nullptr, nullptr, kCrossOriginAttributeNotSet,
     network::mojom::ReferrerPolicy::kDefault, false,
     network::mojom::CredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", nullptr, nullptr,
     kCrossOriginAttributeNotSet, network::mojom::ReferrerPolicy::kDefault,
     true, network::mojom::CredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", nullptr, nullptr,
     kCrossOriginAttributeAnonymous, network::mojom::ReferrerPolicy::kDefault,
     true, network::mojom::CredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", "nonce", nullptr,
     kCrossOriginAttributeNotSet, network::mojom::ReferrerPolicy::kNever, true,
     network::mojom::CredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", nullptr, "sha384-abc",
     kCrossOriginAttributeNotSet, network::mojom::ReferrerPolicy::kDefault,
     true, network::mojom::CredentialsMode::kSameOrigin}};

class LinkLoaderModulePreloadTest
    : public testing::TestWithParam<ModulePreloadTestParams>,
      private ScopedMockOverlayScrollbars {
 private:
  test::TaskEnvironment task_environment_;
};

class ModulePreloadTestModulator final : public DummyModulator {
 public:
  ModulePreloadTestModulator(const ModulePreloadTestParams* params)
      : params_(params), fetched_(false) {}

  void FetchSingle(const ModuleScriptFetchRequest& request,
                   ResourceFetcher*,
                   ModuleGraphLevel,
                   ModuleScriptCustomFetchType custom_fetch_type,
                   SingleModuleClient*) override {
    fetched_ = true;

    EXPECT_EQ(KURL(NullURL(), params_->href), request.Url());
    EXPECT_EQ(params_->nonce, request.Options().Nonce());
    EXPECT_EQ(kNotParserInserted, request.Options().ParserState());
    EXPECT_EQ(params_->expected_credentials_mode,
              request.Options().CredentialsMode());
    EXPECT_EQ(Referrer::NoReferrer(), request.ReferrerString());
    EXPECT_EQ(params_->referrer_policy, request.Options().GetReferrerPolicy());
    EXPECT_EQ(params_->integrity,
              request.Options().GetIntegrityAttributeValue());
    EXPECT_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);
  }

  bool fetched() const { return fetched_; }

 private:
  const ModulePreloadTestParams* params_;
  bool fetched_;
};

TEST_P(LinkLoaderModulePreloadTest, ModulePreload) {
  const auto& test_case = GetParam();
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  ModulePreloadTestModulator* modulator =
      MakeGarbageCollected<ModulePreloadTestModulator>(&test_case);
  Modulator::SetModulator(
      ToScriptStateForMainWorld(dummy_page_holder->GetDocument().GetFrame()),
      modulator);
  Persistent<MockLinkLoaderClient> loader_client =
      MakeGarbageCollected<MockLinkLoaderClient>(true);
  auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
  KURL href_url = KURL(NullURL(), test_case.href);
  LinkLoadParameters params(
      LinkRelAttribute("modulepreload"), test_case.cross_origin,
      String() /* type */, String() /* as */, String() /* media */,
      test_case.nonce, test_case.integrity, String(), test_case.referrer_policy,
      href_url, String() /* image_srcset */, String() /* image_sizes */,
      String() /* blocking */);
  loader->LoadLink(params, dummy_page_holder->GetDocument());
  ASSERT_EQ(test_case.expecting_load, modulator->fetched());
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderModulePreloadTest,
                         LinkLoaderModulePreloadTest,
                         testing::ValuesIn(kModulePreloadTestParams));

class LinkLoaderTestPrefetchPrivacyChanges
    : public testing::Test,
      public testing::WithParamInterface<bool>,
      private ScopedMockOverlayScrollbars {
 public:
  LinkLoaderTestPrefetchPrivacyChanges()
      : privacy_changes_enabled_(GetParam()) {}
  void SetUp() override {
    std::vector<base::test::FeatureRef> enable_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enable_features.push_back(features::kPrefetchPrivacyChanges);
    } else {
      disabled_features.push_back(features::kPrefetchPrivacyChanges);
    }
    feature_list_.InitWithFeatures(enable_features, disabled_features);
  }

 protected:
  const bool privacy_changes_enabled_;
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(LinkLoaderTestPrefetchPrivacyChanges,
                         LinkLoaderTestPrefetchPrivacyChanges,
                         testing::Values(false, true));

TEST_P(LinkLoaderTestPrefetchPrivacyChanges, PrefetchPrivacyChanges) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
  dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
  Persistent<MockLinkLoaderClient> loader_client =
      MakeGarbageCollected<MockLinkLoaderClient>(true);
  auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
  KURL href_url = KURL(NullURL(), "http://example.test/cat.jpg");
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |dummy_page_holder|.
  url_test_helpers::RegisterMockedErrorURLLoad(href_url);
  LinkLoadParameters params(
      LinkRelAttribute("prefetch"), kCrossOriginAttributeNotSet, "image/jpg",
      "", "", "", "", String(), network::mojom::ReferrerPolicy::kDefault,
      href_url, String() /* image_srcset */, String() /* image_sizes */,
      String() /* blocking */);
  loader->LoadLink(params, dummy_page_holder->GetDocument());
  ASSERT_TRUE(dummy_page_holder->GetDocument().Fetcher());
  Resource* resource = loader->GetResourceForTesting();
  EXPECT_TRUE(resource);

  if (privacy_changes_enabled_) {
    EXPECT_EQ(resource->GetResourceRequest().GetRedirectMode(),
              network::mojom::RedirectMode::kError);
    EXPECT_EQ(resource->GetResourceRequest().GetReferrerPolicy(),
              network::mojom::ReferrerPolicy::kNever);
  } else {
    EXPECT_EQ(resource->GetResourceRequest().GetRedirectMode(),
              network::mojom::RedirectMode::kFollow);
    EXPECT_EQ(resource->GetResourceRequest().GetReferrerPolicy(),
              ReferrerUtils::MojoReferrerPolicyResolveDefault(
                  network::mojom::ReferrerPolicy::kDefault));
  }

  URLLoaderMockFactory::GetSingletonInstance()
      ->UnregisterAllURLsAndClearMemoryCache();
}

class LinkLoaderTest : public testing::Test,
                       private ScopedMockOverlayScrollbars {
 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_F(LinkLoaderTest, Prefetch) {
  struct TestCase {
    const char* href;
    // TODO(yoav): Add support for type and media crbug.com/662687
    const char* type;
    const char* media;
    const network::mojom::ReferrerPolicy referrer_policy;
    const bool link_loader_should_load_value;
    const bool expecting_load;
    const network::mojom::ReferrerPolicy expected_referrer_policy;
  } cases[] = {
      // Referrer Policy
      {"http://example.test/cat.jpg", "image/jpg", "",
       network::mojom::ReferrerPolicy::kOrigin, true, true,
       network::mojom::ReferrerPolicy::kOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "",
       network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, true, true,
       network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "",
       network::mojom::ReferrerPolicy::kNever, true, true,
       network::mojom::ReferrerPolicy::kNever},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    auto dummy_page_holder =
        std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
    dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    Persistent<MockLinkLoaderClient> loader_client =
        MakeGarbageCollected<MockLinkLoaderClient>(
            test_case.link_loader_should_load_value);
    auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
    KURL href_url = KURL(NullURL(), test_case.href);
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |dummy_page_holder|.
    url_test_helpers::RegisterMockedErrorURLLoad(href_url);
    LinkLoadParameters params(
        LinkRelAttribute("prefetch"), kCrossOriginAttributeNotSet,
        test_case.type, "", test_case.media, "", "", String(),
        test_case.referrer_policy, href_url, String() /* image_srcset */,
        String() /* image_sizes */, String() /* blocking */);
    loader->LoadLink(params, dummy_page_holder->GetDocument());
    ASSERT_TRUE(dummy_page_holder->GetDocument().Fetcher());
    Resource* resource = loader->GetResourceForTesting();
    if (test_case.expecting_load) {
      EXPECT_TRUE(resource);
    } else {
      EXPECT_FALSE(resource);
    }
    if (resource) {
      if (test_case.expected_referrer_policy !=
          network::mojom::ReferrerPolicy::kDefault) {
        EXPECT_EQ(test_case.expected_referrer_policy,
                  resource->GetResourceRequest().GetReferrerPolicy());
      }
    }
    URLLoaderMockFactory::GetSingletonInstance()
        ->UnregisterAllURLsAndClearMemoryCache();
  }
}

TEST_F(LinkLoaderTest, DNSPrefetch) {
  struct {
    const char* href;
    const bool should_load;
  } cases[] = {
      {"http://example.com/", true},
      {"https://example.com/", true},
      {"//example.com/", true},
      {"//example.com/", false},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    auto dummy_page_holder =
        std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
    dummy_page_holder->GetDocument().GetSettings()->SetDNSPrefetchingEnabled(
        true);
    dummy_page_holder->GetFrame().SetPrescientNetworkingForTesting(
        std::make_unique<NetworkHintsMock>());
    auto* mock_network_hints = static_cast<NetworkHintsMock*>(
        dummy_page_holder->GetFrame().PrescientNetworking());
    Persistent<MockLinkLoaderClient> loader_client =
        MakeGarbageCollected<MockLinkLoaderClient>(test_case.should_load);
    auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
    KURL href_url = KURL(KURL(String("http://example.com")), test_case.href);
    LinkLoadParameters params(
        LinkRelAttribute("dns-prefetch"), kCrossOriginAttributeNotSet, String(),
        String(), String(), String(), String(), String(),
        network::mojom::ReferrerPolicy::kDefault, href_url,
        String() /* image_srcset */, String() /* image_sizes */,
        String() /* blocking */);
    loader->LoadLink(params, dummy_page_holder->GetDocument());
    EXPECT_FALSE(mock_network_hints->DidPreconnect());
    EXPECT_EQ(test_case.should_load, mock_network_hints->DidDnsPrefetch());
  }
}

TEST_F(LinkLoaderTest, Preconnect) {
  struct {
    const char* href;
    CrossOriginAttributeValue cross_origin;
    const bool should_load;
    const bool is_https;
    const bool is_cross_origin;
  } cases[] = {
      {"http://example.com/", kCrossOriginAttributeNotSet, true, false, false},
      {"https://example.com/", kCrossOriginAttributeNotSet, true, true, false},
      {"http://example.com/", kCrossOriginAttributeAnonymous, true, false,
       true},
      {"//example.com/", kCrossOriginAttributeNotSet, true, false, false},
      {"http://example.com/", kCrossOriginAttributeNotSet, false, false, false},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    auto dummy_page_holder =
        std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
    dummy_page_holder->GetFrame().SetPrescientNetworkingForTesting(
        std::make_unique<NetworkHintsMock>());
    auto* mock_network_hints = static_cast<NetworkHintsMock*>(
        dummy_page_holder->GetFrame().PrescientNetworking());
    Persistent<MockLinkLoaderClient> loader_client =
        MakeGarbageCollected<MockLinkLoaderClient>(test_case.should_load);
    auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
    KURL href_url = KURL(KURL(String("http://example.com")), test_case.href);
    LinkLoadParameters params(
        LinkRelAttribute("preconnect"), test_case.cross_origin, String(),
        String(), String(), String(), String(), String(),
        network::mojom::ReferrerPolicy::kDefault, href_url,
        String() /* image_srcset */, String() /* image_sizes */,
        String() /* blocking */);
    loader->LoadLink(params, dummy_page_holder->GetDocument());
    EXPECT_EQ(test_case.should_load, mock_network_hints->DidPreconnect());
    EXPECT_EQ(test_case.is_https, mock_network_hints->IsHTTPS());
    if (test_case.should_load) {
      EXPECT_NE(test_case.is_cross_origin,
                mock_network_hints->AllowCredentials());
    } else {
      EXPECT_EQ(test_case.is_cross_origin,
                mock_network_hints->AllowCredentials());
    }
  }
}

TEST_F(LinkLoaderTest, PreloadAndPrefetch) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
  ResourceFetcher* fetcher = dummy_page_holder->GetDocument().Fetcher();
  ASSERT_TRUE(fetcher);
  dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
  Persistent<MockLinkLoaderClient> loader_client =
      MakeGarbageCollected<MockLinkLoaderClient>(true);
  auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
  KURL href_url = KURL(KURL(), "https://www.example.com/");
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |dummy_page_holder|.
  url_test_helpers::RegisterMockedErrorURLLoad(href_url);
  LinkLoadParameters params(
      LinkRelAttribute("preload prefetch"), kCrossOriginAttributeNotSet,
      "application/javascript", "script", "", "", "", String(),
      network::mojom::ReferrerPolicy::kDefault, href_url,
      String() /* image_srcset */, String() /* image_sizes */,
      String() /* blocking */);
  loader->LoadLink(params, dummy_page_holder->GetDocument());
  ASSERT_EQ(1, fetcher->CountPreloads());
  Resource* resource = loader->GetResourceForTesting();
  ASSERT_NE(resource, nullptr);
  EXPECT_TRUE(resource->IsLinkPreload());
}

class DictionaryLinkTest : public testing::Test,
                           public testing::WithParamInterface<bool> {
 public:
  DictionaryLinkTest()
      : dictionary_scoped_feature_(GetParam()),
        backend_scoped_feature_(GetParam()) {}

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

  void RunIdleTasks() {
    ThreadScheduler::Current()
        ->ToMainThreadScheduler()
        ->StartIdlePeriodForTesting();
    platform_->RunUntilIdle();
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;

 private:
  ScopedCompressionDictionaryTransportForTest dictionary_scoped_feature_;
  ScopedCompressionDictionaryTransportBackendForTest backend_scoped_feature_;
};

INSTANTIATE_TEST_SUITE_P(DictionaryLinkTest,
                         DictionaryLinkTest,
                         testing::Bool());

TEST_P(DictionaryLinkTest, LoadDictionaryFromLink) {
  bool is_dictionary_load_enabled = GetParam();
  static constexpr char href[] = "http://example.test/test.dict";

  // Test the cases with a single header
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
  dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
  Persistent<MockLinkLoaderClient> loader_client =
      MakeGarbageCollected<MockLinkLoaderClient>(is_dictionary_load_enabled);
  auto* loader = MakeGarbageCollected<LinkLoader>(loader_client.Get());
  KURL href_url = KURL(NullURL(), href);
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |dummy_page_holder|.
  url_test_helpers::RegisterMockedErrorURLLoad(href_url);
  LinkLoadParameters params(
      LinkRelAttribute("compression-dictionary"), kCrossOriginAttributeNotSet,
      String() /* type */, String() /* as */, String() /* media */,
      String() /* nonce */, String() /* integrity */,
      String() /* fetch_priority_hint */,
      network::mojom::ReferrerPolicy::kDefault, href_url,
      String() /* image_srcset */, String() /* image_sizes */,
      String() /* blocking */);
  loader->LoadLink(params, dummy_page_holder->GetDocument());
  RunIdleTasks();
  Resource* resource = loader->GetResourceForTesting();
  if (is_dictionary_load_enabled) {
    EXPECT_TRUE(resource);
  } else {
    EXPECT_FALSE(resource);
  }
  URLLoaderMockFactory::GetSingletonInstance()
      ->UnregisterAllURLsAndClearMemoryCache();
}

}  // namespace

// Required to be outside the anomymous namespace for testing
class DictionaryLoadFromHeaderTest : public SimTest,
                                     public testing::WithParamInterface<bool> {
 public:
  DictionaryLoadFromHeaderTest()
      : dictionary_scoped_feature_(GetParam()),
        backend_scoped_feature_(GetParam()) {}

  void SetUp() override {
    SimTest::SetUp();

    SimRequestBase::Params params;
    String link_header =
        String("<") + dict_href_ + ">; rel=\"compression-dictionary\"";
    params.response_http_headers.Set(http_names::kLink, link_header);
    main_resource_ =
        std::make_unique<SimRequest>(page_href_, "text/html", params);
  }

  void RunIdleTasks() {
    ThreadScheduler::Current()
        ->ToMainThreadScheduler()
        ->StartIdlePeriodForTesting();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  static constexpr char page_href_[] = "http://example.test/test.html";
  static constexpr char dict_href_[] = "http://example.test/test.dict";

  std::unique_ptr<SimRequest> main_resource_;

 private:
  ScopedCompressionDictionaryTransportForTest dictionary_scoped_feature_;
  ScopedCompressionDictionaryTransportBackendForTest backend_scoped_feature_;
};

INSTANTIATE_TEST_SUITE_P(DictionaryLoadFromHeaderTest,
                         DictionaryLoadFromHeaderTest,
                         testing::Bool());

TEST_P(DictionaryLoadFromHeaderTest, LoadDictionaryFromHeader) {
  bool is_dictionary_load_enabled = GetParam();

  KURL dict_url = KURL(NullURL(), dict_href_);
  ResourceResponse dict_response(dict_url);
  dict_response.SetHttpStatusCode(200);
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      dict_url, "", WrappedResourceResponse(dict_response));

  LoadURL(page_href_);
  main_resource_->Complete("");

  RunIdleTasks();
  Resource* dictionary_resource =
      GetDocument().GetPendingLinkPreloadForTesting(dict_url);
  ASSERT_EQ(dictionary_resource != nullptr, is_dictionary_load_enabled);
  if (is_dictionary_load_enabled) {
    ASSERT_TRUE(dictionary_resource->IsLoading());
    URLLoaderMockFactory::GetSingletonInstance()->ServeAsynchronousRequests();
    ASSERT_TRUE(dictionary_resource->IsLoaded());
  }
  URLLoaderMockFactory::GetSingletonInstance()
      ->UnregisterAllURLsAndClearMemoryCache();
}

}  // namespace blink
