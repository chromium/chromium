// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/reduce_accept_language/reduce_accept_language_utils.h"

#include <array>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/reduce_accept_language/reduce_accept_language_throttle.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_reduce_accept_language_controller_delegate.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/avail_language_header_parser.h"
#include "services/network/public/cpp/content_language_parser.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

mojo::PendingAssociatedRemote<mojom::Frame> CreateStubFrameRemote() {
  return TestRenderFrameHost::CreateStubFrameRemote();
}

mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
CreateStubBrowserInterfaceBrokerReceiver() {
  return TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver();
}

blink::mojom::PolicyContainerBindParamsPtr
CreateStubPolicyContainerBindParams() {
  return TestRenderFrameHost::CreateStubPolicyContainerBindParams();
}

mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
CreateStubAssociatedInterfaceProviderReceiver() {
  return TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver();
}

static constexpr const char kDeprecationTrialName[] =
    "DisableReduceAcceptLanguage";

}  // namespace

class MockOriginTrialsDelegate
    : public content::OriginTrialsControllerDelegate {
 public:
  ~MockOriginTrialsDelegate() override = default;

  base::flat_map<url::Origin, base::flat_set<std::string>> persisted_trials_;

  base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& origin,
      const url::Origin& top_level_origin,
      base::Time current_time) override {
    return {};
  }

  bool IsFeaturePersistedForOrigin(const url::Origin& origin,
                                   const url::Origin& top_level_origin,
                                   blink::mojom::OriginTrialFeature feature,
                                   const base::Time current_time) override {
    std::string trial_name = "";
    switch (feature) {
      case blink::mojom::OriginTrialFeature::kDisableReduceAcceptLanguage:
        trial_name = kDeprecationTrialName;
        break;
      default:
        break;
    }
    const auto& it = persisted_trials_.find(origin);
    EXPECT_FALSE(trial_name.empty());
    return it != persisted_trials_.end() && it->second.contains(trial_name);
  }

  void PersistTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& top_level_origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override {}

  void PersistAdditionalTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& top_level_origin,
      const base::span<const url::Origin> script_origins,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override {}

  void ClearPersistedTokens() override { persisted_trials_.clear(); }

  void AddPersistedTrialForTest(std::string_view url,
                                std::string_view trial_name) {
    url::Origin key = url::Origin::Create(GURL(url));
    persisted_trials_[key].emplace(trial_name);
  }
};

class AcceptLanguageUtilsTests : public RenderViewHostImplTestHarness {
 public:
  AcceptLanguageUtilsTests()
      : response_headers_(base::MakeRefCounted<net::HttpResponseHeaders>("")) {}
  AcceptLanguageUtilsTests(const AcceptLanguageUtilsTests&) = delete;
  AcceptLanguageUtilsTests& operator=(const AcceptLanguageUtilsTests&) = delete;

  ~AcceptLanguageUtilsTests() override {
    blink::TrialTokenValidator::ResetOriginTrialPolicyGetter();
  }

  static constexpr char kFirstPartyUrl[] = "https://mysite.com:4444";
  static constexpr char kThirdPartyOriginUrl[] = "https://mysite.com:4445";

  void AddOneChildNode() {
    main_test_rfh()->OnCreateChildFrame(
        14, CreateStubFrameRemote(), CreateStubBrowserInterfaceBrokerReceiver(),
        CreateStubPolicyContainerBindParams(),
        CreateStubAssociatedInterfaceProviderReceiver(),
        blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
        false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
        blink::DocumentToken(), blink::FramePolicy(),
        blink::mojom::FrameOwnerProperties(),
        blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  }

  bool ParseAndPersist(const GURL& url,
                       ReduceAcceptLanguageUtils& reduce_language_utils,
                       const std::string& accept_language,
                       const std::string& content_language,
                       const std::string& avail_language) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                      accept_language);
    auto parsed_headers = network::mojom::ParsedHeaders::New();
    parsed_headers->content_language =
        network::ParseContentLanguages(content_language);
    parsed_headers->avail_language =
        network::ParseAvailLanguage(avail_language);

    return reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
        url::Origin::Create(url), headers, parsed_headers);
  }

  const net::HttpResponseHeaders* response_headers() const {
    return response_headers_.get();
  }

 private:
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};

TEST_F(AcceptLanguageUtilsTests, AcceptLanguageMatchContentLanguage) {
  const struct {
    std::string accept_language;
    std::string content_language;
    bool expected_return;
  } tests[] = {
      {"*", "en", false},
      {"en", "*", true},
      {"en", "en", true},
      {"en", "En", true},
      {"En", "en", true},
      {"en", "en-us", false},
      {"en-us", "en", true},
      {"de-de", "de-de-1996", false},
      {"de-De-1996", "de-de", true},
      {"de-de", "de-Deva", false},
      {"de-de", "de-Latn-DE", false},
  };

  for (const auto& test : tests) {
    bool actual_return =
        ReduceAcceptLanguageUtils::DoesAcceptLanguageMatchContentLanguage(
            test.accept_language, test.content_language);
    EXPECT_EQ(test.expected_return, actual_return)
        << "Test case: {" << test.accept_language << ", "
        << test.content_language << "}: expected return "
        << test.expected_return << " but got " << actual_return << ".";
  }
}

TEST_F(AcceptLanguageUtilsTests, OriginCanReduceAcceptLanguage) {
  struct TestCase {
    std::string origin;
    bool expected_return;
  };

  const auto tests = std::to_array<TestCase>({
      {"http://example.com", true},
      {"https://example.com", true},
      {"ws://example.com", false},
      {"http://example.com/1.jpg", true},
      {"https://example.com/1.jpg", true},
  });

  for (size_t i = 0; i < std::size(tests); ++i) {
    bool actual_return =
        ReduceAcceptLanguageUtils::OriginCanReduceAcceptLanguage(
            url::Origin::Create(GURL(tests[i].origin)));
    EXPECT_EQ(tests[i].expected_return, actual_return)
        << "Test case " << i << ": expected return " << tests[i].expected_return
        << " but got " << actual_return << ".";
  }
}

TEST_F(AcceptLanguageUtilsTests, FirstMatchPreferredLang) {
  struct TestCase {
    std::vector<std::string> preferred_languages;
    std::vector<std::string> available_languages;
    std::optional<std::string> expected_match_language;
  };

  const auto tests = std::to_array<TestCase>({
      {{}, {"en"}, std::nullopt},
      {{}, {"*"}, std::nullopt},
      {{"en"}, {}, std::nullopt},
      {{"en"}, {"en"}, "en"},
      {{"en"}, {"*"}, "en"},
      {{"en"}, {"en-US"}, std::nullopt},
      {{"en-us"}, {"en"}, std::nullopt},
      {{"en-us"}, {"en-US"}, "en-us"},
      {{"en-us", "ja", "en"}, {"ja", "en-us"}, "en-us"},
  });

  for (size_t i = 0; i < std::size(tests); ++i) {
    std::optional<std::string> actual_matched_language =
        ReduceAcceptLanguageUtils::GetFirstMatchPreferredLanguage(
            tests[i].preferred_languages, tests[i].available_languages);

    EXPECT_EQ(tests[i].expected_match_language, actual_matched_language)
        << "Test case " << i << ": expected matched language "
        << (tests[i].expected_match_language
                ? tests[i].expected_match_language.value()
                : "nullptr")
        << " but got "
        << (actual_matched_language ? actual_matched_language.value()
                                    : "nullptr")
        << ".";
  }
}

TEST_F(AcceptLanguageUtilsTests, AddNavigationRequestAcceptLanguageHeaders) {
  base::test::ScopedFeatureList scoped_feature_list;

  MockReduceAcceptLanguageControllerDelegate delegate =
      MockReduceAcceptLanguageControllerDelegate("en,zh");
  ReduceAcceptLanguageUtils reduce_language_utils(delegate);

  GURL url = GURL("https://example.com");
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();
  {
    // Verify no header added when feature turns off.
    net::HttpRequestHeaders headers;
    std::optional<std::string> added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), root, &headers);
    EXPECT_FALSE(headers.HasHeader(net::HttpRequestHeaders::kAcceptLanguage));
    EXPECT_FALSE(added_accept_language.has_value());
  }

  // Test add navigation header with reduce accept language feature turns on.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguage}, {});
  {
    // Verify root frame node has the accept language header.
    net::HttpRequestHeaders headers;
    std::optional<std::string> added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), root, &headers);
    EXPECT_THAT(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage),
                testing::Optional(std::string("en")));
    EXPECT_EQ("en", added_accept_language.value());

    // Verify child node still has the accept language header.
    AddOneChildNode();
    FrameTreeNode* child0 = root->child_at(0);

    net::HttpRequestHeaders child_http_headers;
    added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), child0, &child_http_headers);

    EXPECT_THAT(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage),
                testing::Optional(std::string("en")));
    EXPECT_EQ("en", added_accept_language.value());

    // Verify use the persist language when it's available instead of user first
    // accept language.
    std::string test_persisted_lang = "en";
    delegate.PersistReducedLanguage(url::Origin::Create(url),
                                    test_persisted_lang);

    added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), root, &headers);
    EXPECT_THAT(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage),
                testing::Optional(test_persisted_lang));
    EXPECT_EQ(test_persisted_lang, added_accept_language.value());
    // Verify commit language has the same value.
    std::optional<std::string> commit_lang =
        reduce_language_utils.LookupReducedAcceptLanguage(
            url::Origin::Create(url), root);
    EXPECT_TRUE(commit_lang.has_value());
    EXPECT_EQ(test_persisted_lang, commit_lang.value());

    // Verify persist language is not one of user's accepted languages, use
    // user's first accepted language instead.
    test_persisted_lang = "ja";
    delegate.PersistReducedLanguage(url::Origin::Create(url),
                                    test_persisted_lang);

    added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), root, &headers);
    EXPECT_THAT(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage),
                testing::Optional(std::string("en")));
    EXPECT_EQ("en", added_accept_language.value());
  }
}

TEST_F(AcceptLanguageUtilsTests, ParseAndPersistAcceptLanguageForNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguage}, {});

  GURL url = GURL("https://example.com");
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* child0 = root->child_at(0);

  {
    // Verify parse return correct values.
    MockReduceAcceptLanguageControllerDelegate delegate =
        MockReduceAcceptLanguageControllerDelegate("en,zh");
    ReduceAcceptLanguageUtils reduce_language_utils(delegate);
    net::HttpRequestHeaders headers;
    auto parsed_headers = network::mojom::ParsedHeaders::New();

    // Expect return false when parsed_headers->content_language is null.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(url), headers, parsed_headers));

    parsed_headers->content_language = network::ParseContentLanguages("en");
    // Expect return false when parsed_headers->avail_language is null.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(url), headers, parsed_headers));

    parsed_headers->avail_language = network::ParseAvailLanguage(" ");
    // Expect return false when invalid URL.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(GURL("ws://example.com")), headers,
            parsed_headers));

    // Expect return false when parsed_headers->avail_language has no
    // accept-language values.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(GURL(url)), headers, parsed_headers));

    parsed_headers->avail_language = network::ParseAvailLanguage("en, zh");
    // Expect return false when no initial accept-language header.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(url), headers, parsed_headers));

    headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage, "en");
    // Site supports user's accept language. Accept-Language matches
    // Content-Language, no need to restart and persist language.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(url), headers, parsed_headers));
    std::optional<std::string> persisted_lang =
        delegate.GetReducedLanguage(url::Origin::Create(url));
    EXPECT_FALSE(persisted_lang.has_value());
  }

  {
    // Verify incognito only return first user accept language.
    MockReduceAcceptLanguageControllerDelegate delegate =
        MockReduceAcceptLanguageControllerDelegate("en,zh", true);
    EXPECT_EQ(std::vector<std::string>{"en"},
              delegate.GetUserAcceptLanguages());

    MockReduceAcceptLanguageControllerDelegate delegate1 =
        MockReduceAcceptLanguageControllerDelegate("en-US,zh", true);
    EXPECT_EQ(std::vector<std::string>({"en-US", "en"}),
              delegate1.GetUserAcceptLanguages());
  }

  // Verify parse and persist languages in different combinations.
  {
    const struct {
      std::string user_accept_languages;
      std::string accept_language;
      std::string content_language;
      std::string avail_language;
      bool expected_resend_request;
      std::optional<std::string> expected_persisted_language;
      std::optional<std::string> expected_commit_language;
    } tests[] = {
        // Test cases for special language values.
        {"en,zh", "en", "ja", "ja, unknown", false, std::nullopt, "en"},
        {"en,zh", "en", "ja", "*", true, std::nullopt, "en"},
        {"zh,en", "", "ja", "ja, en", true, "en", "en"},
        {"en,zh", "en", "ja", "INVALID", false, std::nullopt, "en"},
        // Test cases for multiple content languages
        {"en,zh", "zh", "zh, ja", "en, zh", false, "zh", "zh"},
        {"en,zh", "en", "zh, en", "en, ja, zh", false, std::nullopt, "en"},
        {"en,zh", "en", "zh, en", "ja, en;d, zh", false, std::nullopt, "en"},
        {"en,zh", "en", "es, ja", "es, ja, zh", true, "zh", "zh"},
        // Test cases for base language without country code.
        {"en,zh", "zh", "zh", "en, zh", false, "zh", "zh"},
        {"en,zh", "en", "zh", "ja, zh", false, "zh", "zh"},
        {"en,ja,zh", "en", "zh", "ja, zh", true, "ja", "ja"},
        {"en,zh", "en", "ja", "ja", false, std::nullopt, "en"},
        {"zh,en", "zh", "ja", "ja, en", true, "en", "en"},
        // Test cases mix with base language and language with country code.
        {"zh,en-US", "zh", "ja", "ja, en", true, "en", "en"},
        {"en,zh-CN", "en", "zh-cn", "ja, zh-CN", false, "zh-CN", "zh-CN"},
        {"en-US,zh", "en-US", "ja", "ja, en", true, "en", "en"},
        {"en-US,zh", "en-US", "ja", "ja, en-GB", false, std::nullopt, "en-US"},
        // Test cases with language-region pair has big difference in language.
        {"zh", "zh", "zh-HK", "zh-HK", false, std::nullopt, "zh"},
        {"zh-HK", "zh-HK", "zh", "zh", false, std::nullopt, "zh-HK"},
        {"zh-CN", "zh-CN", "zh-HK", "zh-HK", false, std::nullopt, "zh-CN"},
        {"zh-CN,zh", "zh-CN", "zh-HK", "zh-HK", false, std::nullopt, "zh-CN"},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "zh-HK", false, "zh-HK", "zh-HK"},
        {"zh-CN,zh", "zh-CN", "zh-HK", "zh-HK, zh", true, "zh", "zh"},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "zh-HK, zh-CN, zh", true,
         std::nullopt, "zh-CN"},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "zh-HK, zh, zh-CN", true,
         std::nullopt, "zh-CN"},
        // Test cases with empty user accept-language we ignore reduce the
        // Accept-Language HTTP header.
        {"", "zh", "ja", "ja, en", false, std::nullopt, std::nullopt},
    };

    size_t i = 0;
    for (const auto& test : tests) {
      MockReduceAcceptLanguageControllerDelegate delegate =
          MockReduceAcceptLanguageControllerDelegate(
              test.user_accept_languages);
      ReduceAcceptLanguageUtils reduce_language_utils(delegate);
      // Verify whether needs to resend request
      bool actual_resend_request =
          ParseAndPersist(url, reduce_language_utils,
                          /*accept_language=*/test.accept_language,
                          /*content_language=*/test.content_language,
                          /*avail_language=*/test.avail_language);
      EXPECT_EQ(test.expected_resend_request, actual_resend_request)
          << "Test case " << i << ": expected resend request "
          << test.expected_resend_request << " but got "
          << actual_resend_request << ".";
      // Verify persist language

      std::optional<std::string> actual_persisted_language =
          delegate.GetReducedLanguage(url::Origin::Create(url));
      EXPECT_EQ(test.expected_persisted_language, actual_persisted_language)
          << "Test case " << i << ": expected persisted language "
          << (test.expected_persisted_language
                  ? test.expected_persisted_language.value()
                  : "nullopt")
          << " but got "
          << (actual_persisted_language ? actual_persisted_language.value()
                                        : "nullopt")
          << ".";

      // Verify commit language
      std::optional<std::string> actual_commit_language =
          reduce_language_utils.LookupReducedAcceptLanguage(
              url::Origin::Create(url), root);
      EXPECT_EQ(test.expected_commit_language, actual_commit_language)
          << "Test case " << i << ": expected commit language "
          << (test.expected_commit_language
                  ? test.expected_commit_language.value()
                  : "nullopt")
          << " but got "
          << (actual_commit_language ? actual_commit_language.value()
                                     : "nullopt")
          << ".";

      // Verify child frame commit language same as parent commit language.
      std::optional<std::string> actual_child_commit_language =
          reduce_language_utils.LookupReducedAcceptLanguage(
              url::Origin::Create(url), child0);
      EXPECT_EQ(test.expected_commit_language, actual_child_commit_language)
          << "Test case " << i << ": expected child frame commit language "
          << (test.expected_commit_language
                  ? test.expected_commit_language.value()
                  : "nullopt")
          << " but got "
          << (actual_child_commit_language
                  ? actual_child_commit_language.value()
                  : "nullopt")
          << ".";
      i++;
    }
  }
}

TEST_F(AcceptLanguageUtilsTests, VerifyClearAcceptLanguage) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguage}, {});

  GURL url = GURL("https://example.com");
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();

  MockReduceAcceptLanguageControllerDelegate delegate =
      MockReduceAcceptLanguageControllerDelegate("zh,ja,en-US");
  ReduceAcceptLanguageUtils reduce_language_utils(delegate);

  ParseAndPersist(url, reduce_language_utils,
                  /*accept_language=*/"zh",
                  /*content_language=*/"es",
                  /*avail_language=*/"ja, es;d, en-US");

  // Verify persisted reduce accept-language is "ja".
  url::Origin origin = url::Origin::Create(url);
  std::optional<std::string> actual_persisted_language =
      delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  std::optional<std::string> actual_commit_language =
      reduce_language_utils.LookupReducedAcceptLanguage(origin, root);
  EXPECT_EQ("ja", actual_commit_language);

  // Update user language preference list to not include "ja".
  delegate.SetUserAcceptLanguages("zh,en-US");
  // Verify commit language is the first language in user's preference list.
  std::optional<std::string> new_commit_language =
      reduce_language_utils.LookupReducedAcceptLanguage(origin, root);
  EXPECT_EQ("zh", new_commit_language);
  // Verify persist language has been cleared once user accept language list
  // updates.
  std::optional<std::string> new_persisted_language =
      delegate.GetReducedLanguage(origin);
  EXPECT_FALSE(new_persisted_language.has_value());
}

TEST_F(AcceptLanguageUtilsTests, ValidateDeprecationOriginTrial) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguage}, {});

  GURL request_url = GURL(kFirstPartyUrl);
  contents()->NavigateAndCommit(request_url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();

  MockOriginTrialsDelegate origin_trials_delegate;
  origin_trials_delegate.AddPersistedTrialForTest(kFirstPartyUrl,
                                                  kDeprecationTrialName);
  EXPECT_TRUE(
      ReduceAcceptLanguageUtils::CheckDisableReduceAcceptLanguageOriginTrial(
          request_url, root, &origin_trials_delegate));
  EXPECT_FALSE(
      ReduceAcceptLanguageUtils::CheckDisableReduceAcceptLanguageOriginTrial(
          request_url, nullptr, &origin_trials_delegate));
  EXPECT_FALSE(
      ReduceAcceptLanguageUtils::CheckDisableReduceAcceptLanguageOriginTrial(
          request_url, root, nullptr));
}

TEST_F(AcceptLanguageUtilsTests, ThrottleProcessResponse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguage}, {});

  MockReduceAcceptLanguageControllerDelegate delegate =
      MockReduceAcceptLanguageControllerDelegate("en,zh");

  GURL request_url = GURL(kFirstPartyUrl);
  contents()->NavigateAndCommit(request_url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();

  MockOriginTrialsDelegate origin_trials_delegate;
  ReduceAcceptLanguageThrottle throttle = ReduceAcceptLanguageThrottle(
      delegate, &origin_trials_delegate, root->frame_tree_node_id());

  // User's first prefer language.
  std::string language = delegate.GetUserAcceptLanguages()[0];

  network::ResourceRequest request;
  request.url = request_url;
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage, language);
  request.headers = headers;
  bool defer = false;
  blink::URLLoaderThrottle::RestartWithURLReset restart_with_url_reset(false);
  throttle.WillStartRequest(&request, &defer);

  delegate.PersistReducedLanguage(url::Origin::Create(request_url), language);

  network::mojom::URLResponseHead response_head;
  response_head.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // Early returns without the avail-language header.
  {
    throttle.BeforeWillProcessResponse(request_url, response_head,
                                       &restart_with_url_reset);
    std::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), language);
  }

  // Early returns with service worker.
  response_head.parsed_headers = network::mojom::ParsedHeaders::New();
  response_head.parsed_headers->content_language =
      network::ParseContentLanguages("ja");
  response_head.parsed_headers->avail_language =
      network::ParseAvailLanguage("ja, zh");
  {
    response_head.did_service_worker_navigation_preload = true;
    throttle.BeforeWillProcessResponse(request_url, response_head,
                                       &restart_with_url_reset);

    std::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), language);
  }

  // Early return when the deprecation trial feature turned on.
  {
    origin_trials_delegate.AddPersistedTrialForTest(kFirstPartyUrl,
                                                    kDeprecationTrialName);
    throttle.BeforeWillProcessResponse(request_url, response_head,
                                       &restart_with_url_reset);

    std::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), language);
    origin_trials_delegate.ClearPersistedTokens();
  }

  // ReduceAcceptLanguageThrottle reads and parses the language: the persisted
  // language should be updated to new negotiated language `zh`.
  // User's initial accept-language is `en`, user full accept-language is
  // [`en`, `zh`], and site supports language list is [`ja`, `zh`]. After
  // language negotiation, the user's preferred language is `zh`.
  {
    response_head.did_service_worker_navigation_preload = false;
    throttle.BeforeWillProcessResponse(request_url, response_head,
                                       &restart_with_url_reset);

    std::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), "zh");
  }
}

class CreateAcceptLanguageUtilsTest : public ::testing::Test {
 public:
  CreateAcceptLanguageUtilsTest() = default;
  CreateAcceptLanguageUtilsTest(const CreateAcceptLanguageUtilsTest&) = delete;
  CreateAcceptLanguageUtilsTest& operator=(
      const CreateAcceptLanguageUtilsTest&) = delete;

  TestBrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
};

TEST_F(CreateAcceptLanguageUtilsTest, CreateUtils) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguage}, {});

  EXPECT_EQ(ReduceAcceptLanguageUtils::Create(browser_context()), std::nullopt);

  // Expect return a valid instance.
  browser_context()->SetReduceAcceptLanguageControllerDelegate(
      std::make_unique<MockReduceAcceptLanguageControllerDelegate>("en,zh"));
  EXPECT_NE(ReduceAcceptLanguageUtils::Create(browser_context()), std::nullopt);

  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {}, {network::features::kReduceAcceptLanguage});
  // Feature reset should expect no instance returns
  EXPECT_EQ(ReduceAcceptLanguageUtils::Create(browser_context()), std::nullopt);
}

}  // namespace content
