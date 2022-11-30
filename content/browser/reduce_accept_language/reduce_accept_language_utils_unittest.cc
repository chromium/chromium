// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/reduce_accept_language/reduce_accept_language_utils.h"
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
#include "services/network/public/cpp/content_language_parser.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/variants_header_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_public_key.h"
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

// generate_token.py https://mysite.com:4444 ReduceAcceptLanguage
// --expire-timestamp=2000000000
static constexpr const char kFirstPartyOriginToken[] =
    "A3NfKLda8C/YMd/Kv+xVm9EDScIvq6t1DYkX57e6W3EZDeqM/G9bMRWHzN/aS/"
    "Hd8VMtLXuY9WGdN7gesHbqSgEAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly9teXNpdGUuY29t"
    "Oj"
    "Q0NDQiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOiAy"
    "MD"
    "AwMDAwMDAwfQ==";

// generate_token.py https://mysite.com:4445 ReduceAcceptLanguage
// --is-third-party --expire-timestamp=2000000000
static constexpr const char kThirdPartyOriginToken[] =
    "A53cWhGwxKeD4ta+qpdpFUR5WJK/v8sHBLtctggIgUefN1/"
    "A1H0OxU3ISVxCuSCNefWIKpg5BDB3LhMf28qnGAUAAAB0eyJvcmlnaW4iOiAiaHR0cHM6L"
    "y9"
    "teXNpdGUuY29tOjQ0NDUiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsI"
    "CJ"
    "leHBpcnkiOiAyMDAwMDAwMDAwLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZX0=";

static constexpr const char kInvalidOriginToken[] =
    "AjfC47H1q8/Ho5ALFkjkwf9CBK6oUUeRTlFc50Dj+eZEyGGKFIY2WTxMBfy8cLc3"
    "E0nmFroDA3OmABmO5jMCFgkAAABXeyJvcmlnaW4iOiAiaHR0cDovL3ZhbGlkLmV4"
    "YW1wbGUuY29tOjgwIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6"
    "IDIwMDAwMDAwMDB9";

}  // namespace

class TestOriginTrialPolicy : public blink::OriginTrialPolicy {
 public:
  bool IsOriginTrialsSupported() const override { return true; }
  bool IsOriginSecure(const GURL& url) const override {
    return url.SchemeIs("https");
  }
  const std::vector<blink::OriginTrialPublicKey>& GetPublicKeys()
      const override {
    return keys_;
  }
  void SetPublicKeys(const std::vector<blink::OriginTrialPublicKey>& keys) {
    keys_ = keys;
  }

 private:
  std::vector<blink::OriginTrialPublicKey> keys_;
};

class AcceptLanguageUtilsTests : public RenderViewHostImplTestHarness {
 public:
  AcceptLanguageUtilsTests()
      : response_headers_(base::MakeRefCounted<net::HttpResponseHeaders>("")) {
    blink::TrialTokenValidator::SetOriginTrialPolicyGetter(base::BindRepeating(
        [](blink::OriginTrialPolicy* policy) { return policy; },
        base::Unretained(&policy_)));
    policy_.SetPublicKeys({kTestPublicKey});
  }
  AcceptLanguageUtilsTests(const AcceptLanguageUtilsTests&) = delete;
  AcceptLanguageUtilsTests& operator=(const AcceptLanguageUtilsTests&) = delete;

  ~AcceptLanguageUtilsTests() override {
    blink::TrialTokenValidator::ResetOriginTrialPolicyGetter();
  }

  static constexpr blink::OriginTrialPublicKey kTestPublicKey = {
      0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
      0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
      0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
  };

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
                       const std::string& variants_accept_language,
                       bool is_origin_trial_enabled) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                      accept_language);
    auto parsed_headers = network::mojom::ParsedHeaders::New();
    parsed_headers->content_language =
        network::ParseContentLanguages(content_language);
    parsed_headers->variants_headers = network::ParseVariantsHeaders(
        "accept-language=" + variants_accept_language);

    return reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
        url::Origin::Create(url), headers, parsed_headers,
        is_origin_trial_enabled);
  }

  const net::HttpResponseHeaders* response_headers() const {
    return response_headers_.get();
  }

  void SetOriginTrialTokenHeader(const std::string& token) {
    response_headers_->SetHeader("Origin-Trial", token);
  }

 private:
  TestOriginTrialPolicy policy_;
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

  for (size_t i = 0; i < std::size(tests); ++i) {
    bool actual_return =
        ReduceAcceptLanguageUtils::DoesAcceptLanguageMatchContentLanguage(
            tests[i].accept_language, tests[i].content_language);
    EXPECT_EQ(tests[i].expected_return, actual_return)
        << "Test case " << i << ": expected return " << tests[i].expected_return
        << " but got " << actual_return << ".";
  }
}

TEST_F(AcceptLanguageUtilsTests, OriginCanReduceAcceptLanguage) {
  const struct {
    std::string origin;
    bool expected_return;
  } tests[] = {{"http://example.com", true},
               {"https://example.com", true},
               {"ws://example.com", false},
               {"http://example.com/1.jpg", true},
               {"https://example.com/1.jpg", true}};

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
  const struct {
    std::vector<std::string> preferred_languages;
    std::vector<std::string> available_languages;
    absl::optional<std::string> expected_match_language;
  } tests[] = {
      {{}, {"en"}, absl::nullopt},
      {{}, {"*"}, absl::nullopt},
      {{"en"}, {}, absl::nullopt},
      {{"en"}, {"en"}, "en"},
      {{"en"}, {"*"}, "en"},
      {{"en"}, {"en-US"}, absl::nullopt},
      {{"en-us"}, {"en"}, absl::nullopt},
      {{"en-us"}, {"en-US"}, "en-us"},
      {{"en-us", "ja", "en"}, {"ja", "en-us"}, "en-us"},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    absl::optional<std::string> actual_matched_language =
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
    absl::optional<std::string> added_accept_language =
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
    absl::optional<std::string> added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), root, &headers);
    std::string accept_language_header;
    EXPECT_TRUE(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                  &accept_language_header));
    EXPECT_EQ("en", accept_language_header);
    EXPECT_EQ("en", added_accept_language.value());

    // Verify child node still has the accept language header.
    AddOneChildNode();
    FrameTreeNode* child0 = root->child_at(0);

    net::HttpRequestHeaders child_http_headers;
    added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), child0, &child_http_headers);

    accept_language_header.clear();
    EXPECT_TRUE(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                  &accept_language_header));
    EXPECT_EQ("en", accept_language_header);
    EXPECT_EQ("en", added_accept_language.value());

    // Verify use the persist language when it's available instead of user first
    // accept language.
    std::string test_persisted_lang = "en";
    delegate.PersistReducedLanguage(url::Origin::Create(url),
                                    test_persisted_lang);

    added_accept_language =
        reduce_language_utils.AddNavigationRequestAcceptLanguageHeaders(
            url::Origin::Create(url), root, &headers);
    accept_language_header.clear();
    EXPECT_TRUE(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                  &accept_language_header));
    EXPECT_EQ(test_persisted_lang, accept_language_header);
    EXPECT_EQ(test_persisted_lang, added_accept_language.value());
    // Verify commit language has the same value.
    absl::optional<std::string> commit_lang =
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
    accept_language_header.clear();
    EXPECT_TRUE(headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                  &accept_language_header));
    EXPECT_EQ("en", accept_language_header);
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
    // Expect return false when parsed_headers->variants_headers is null.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(url), headers, parsed_headers));

    parsed_headers->variants_headers = network::ParseVariantsHeaders(" ");
    // Expect return false when invalid URL.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(GURL("ws://example.com")), headers,
            parsed_headers));

    // Expect return false when parsed_headers->variants_headers has no
    // accept-language values.
    EXPECT_FALSE(
        reduce_language_utils.ReadAndPersistAcceptLanguageForNavigation(
            url::Origin::Create(GURL(url)), headers, parsed_headers));

    parsed_headers->variants_headers =
        network::ParseVariantsHeaders("accept-language=(en zh)");
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
    absl::optional<std::string> persisted_lang =
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
      std::string variants_accept_language;
      bool expected_resend_request;
      absl::optional<std::string> expected_persisted_language;
      absl::optional<std::string> expected_commit_language;
      bool is_origin_trial_enabled = false;
    } tests[] = {
        // Test cases for special language values.
        {"en,zh", "en", "ja", "(ja unknown)", false, absl::nullopt, "en"},
        {"en,zh", "en", "ja", "(*)", true, absl::nullopt, "en"},
        {"zh,en", "", "ja", "(ja en)", true, "en", "en"},
        {"en,zh", "en", "ja", "INVALID", false, absl::nullopt, "en"},
        // Test cases for multiple content languages
        {"en,zh", "zh", "zh, ja", "(en zh)", false, "zh", "zh"},
        {"en,zh", "en", "zh, en", "(en ja zh)", false, absl::nullopt, "en"},
        {"en,zh", "en", "es, ja", "(es ja zh)", true, "zh", "zh"},
        // Test cases for base language without country code.
        {"en,zh", "zh", "zh", "(en zh)", false, "zh", "zh"},
        {"en,zh", "en", "zh", "(ja zh)", false, "zh", "zh"},
        {"en,ja,zh", "en", "zh", "(ja zh)", true, "ja", "ja"},
        {"en,zh", "en", "ja", "(ja)", false, absl::nullopt, "en"},
        {"zh,en", "zh", "ja", "(ja en)", true, "en", "en"},
        // Test cases mix with base language and language with country code.
        {"zh,en-US", "zh", "ja", "(ja en)", true, "en", "en"},
        {"en,zh-CN", "en", "zh-cn", "(ja zh-CN)", false, "zh-CN", "zh-CN"},
        {"en-US,zh", "en-US", "ja", "(ja en)", true, "en", "en"},
        {"en-US,zh", "en-US", "ja", "(ja en-GB)", false, absl::nullopt,
         "en-US"},
        // Test cases with language-region pair has big difference in language.
        {"zh", "zh", "zh-HK", "(zh-HK)", false, absl::nullopt, "zh"},
        {"zh-HK", "zh-HK", "zh", "(zh)", false, absl::nullopt, "zh-HK"},
        {"zh-CN", "zh-CN", "zh-HK", "(zh-HK)", false, absl::nullopt, "zh-CN"},
        {"zh-CN,zh", "zh-CN", "zh-HK", "(zh-HK)", false, absl::nullopt,
         "zh-CN"},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "(zh-HK)", false, "zh-HK",
         "zh-HK"},
        {"zh-CN,zh", "zh-CN", "zh-HK", "(zh-HK zh)", true, "zh", "zh"},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "(zh-HK zh-CN zh)", true,
         absl::nullopt, "zh-CN"},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "(zh-HK zh zh-CN)", true,
         absl::nullopt, "zh-CN"},
        // Test cases with origin trial enable.
        {"en,zh", "en", "ja", "(ja unknown)", false, "en", "en", true},
        {"en,zh", "en", "ja", "(*)", true, "en", "en", true},
        {"en,zh", "en", "zh, en", "(en ja zh)", false, "en", "en", true},
        {"zh-HK", "zh-HK", "zh", "(zh)", false, "zh-HK", "zh-HK", true},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "(zh-HK zh-CN zh)", true, "zh-CN",
         "zh-CN", true},
        {"zh-CN,zh,zh-HK", "zh-CN", "zh-HK", "(zh-HK zh zh-CN)", true, "zh-CN",
         "zh-CN", true},
    };

    for (size_t i = 0; i < std::size(tests); ++i) {
      MockReduceAcceptLanguageControllerDelegate delegate =
          MockReduceAcceptLanguageControllerDelegate(
              tests[i].user_accept_languages);
      ReduceAcceptLanguageUtils reduce_language_utils(delegate);
      // Verify whether needs to resend request
      bool actual_resend_request = ParseAndPersist(
          url, reduce_language_utils,
          /*accept_language=*/tests[i].accept_language,
          /*content_language=*/tests[i].content_language,
          /*variants_accept_language=*/tests[i].variants_accept_language,
          /*is_origin_trial_enabled=*/tests[i].is_origin_trial_enabled);
      EXPECT_EQ(tests[i].expected_resend_request, actual_resend_request)
          << "Test case " << i << ": expected resend request "
          << tests[i].expected_resend_request << " but got "
          << actual_resend_request << ".";
      // Verify persist language

      absl::optional<std::string> actual_persisted_language =
          delegate.GetReducedLanguage(url::Origin::Create(url));
      EXPECT_EQ(tests[i].expected_persisted_language, actual_persisted_language)
          << "Test case " << i << ": expected persisted language "
          << (tests[i].expected_persisted_language
                  ? tests[i].expected_persisted_language.value()
                  : "nullopt")
          << " but got "
          << (actual_persisted_language ? actual_persisted_language.value()
                                        : "nullopt")
          << ".";

      // Verify commit language
      absl::optional<std::string> actual_commit_language =
          reduce_language_utils.LookupReducedAcceptLanguage(
              url::Origin::Create(url), root);
      EXPECT_EQ(tests[i].expected_commit_language, actual_commit_language)
          << "Test case " << i << ": expected commit language "
          << (tests[i].expected_commit_language
                  ? tests[i].expected_commit_language.value()
                  : "nullopt")
          << " but got "
          << (actual_commit_language ? actual_commit_language.value()
                                     : "nullopt")
          << ".";

      // Verify child frame commit language same as parent commit language.
      absl::optional<std::string> actual_child_commit_language =
          reduce_language_utils.LookupReducedAcceptLanguage(
              url::Origin::Create(url), child0);
      EXPECT_EQ(tests[i].expected_commit_language, actual_child_commit_language)
          << "Test case " << i << ": expected child frame commit language "
          << (tests[i].expected_commit_language
                  ? tests[i].expected_commit_language.value()
                  : "nullopt")
          << " but got "
          << (actual_child_commit_language
                  ? actual_child_commit_language.value()
                  : "nullopt")
          << ".";
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
                  /*variants_accept_language=*/"(es ja en-US)",
                  /*is_origin_trial_enabled=*/false);

  // Verify persisted reduce accept-language is "ja".
  url::Origin origin = url::Origin::Create(url);
  absl::optional<std::string> actual_persisted_language =
      delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  absl::optional<std::string> actual_commit_language =
      reduce_language_utils.LookupReducedAcceptLanguage(origin, root);
  EXPECT_EQ("ja", actual_commit_language);

  // Update user language preference list to not include "ja".
  delegate.SetUserAcceptLanguages("zh,en-US");
  // Verify commit language is the first language in user's preference list.
  absl::optional<std::string> new_commit_language =
      reduce_language_utils.LookupReducedAcceptLanguage(origin, root);
  EXPECT_EQ("zh", new_commit_language);
  // Verify persist language has been cleared once user accept language list
  // updates.
  absl::optional<std::string> new_persisted_language =
      delegate.GetReducedLanguage(origin);
  EXPECT_FALSE(new_persisted_language.has_value());
}

TEST_F(AcceptLanguageUtilsTests, VerifyRemoveOriginTrialStorage) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguageOriginTrial},
      {network::features::kReduceAcceptLanguage});

  GURL url = GURL(kFirstPartyUrl);
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* child0 = root->child_at(0);

  MockReduceAcceptLanguageControllerDelegate delegate =
      MockReduceAcceptLanguageControllerDelegate("zh,ja,en-US");
  ReduceAcceptLanguageUtils reduce_language_utils(delegate);

  ParseAndPersist(url, reduce_language_utils,
                  /*accept_language=*/"zh",
                  /*content_language=*/"es",
                  /*variants_accept_language=*/"(es ja en-US)",
                  /*is_origin_trial_enabled=*/false);

  network::mojom::URLResponseHead response;

  url::Origin origin = url::Origin::Create(url);
  absl::optional<std::string> actual_persisted_language =
      delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  url::Origin opaque_origin;
  reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage(
      actual_persisted_language.value(), opaque_origin, &response, root);
  // Expect no remove for opaque origin.
  actual_persisted_language = delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  // Expect no remove for null response header.
  reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage(
      actual_persisted_language.value(), origin, nullptr, root);
  actual_persisted_language = delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  // Expect no remove for empty response header.
  reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage(
      actual_persisted_language.value(), origin, &response, root);
  actual_persisted_language = delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  // Expect no remove for non main frame.
  response.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage(
      actual_persisted_language.value(), origin, &response, child0);
  actual_persisted_language = delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  // Expect no remove for empty persisted language.
  reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage("", origin,
                                                               &response, root);
  actual_persisted_language = delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  // Expect no remove for response header with valid origin token.
  response.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
  response.headers->SetHeader("Origin-Trial", kFirstPartyOriginToken);
  EXPECT_TRUE(ReduceAcceptLanguageUtils::IsReduceAcceptLanguageEnabledForOrigin(
      origin, response.headers.get()));
  reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage(
      actual_persisted_language.value(), origin, &response, root);
  actual_persisted_language = delegate.GetReducedLanguage(origin);
  EXPECT_EQ("ja", actual_persisted_language.value());

  // Expect no remove if the kReduceAcceptLanguage feature is enabled.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguage}, {});
  {
    response.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
    response.headers->SetHeader("Origin-Trial", kInvalidOriginToken);
    reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage(
        actual_persisted_language.value(), origin, &response, root);
    actual_persisted_language = delegate.GetReducedLanguage(origin);
    EXPECT_EQ("ja", actual_persisted_language.value());
  }

  // Expect remove for invalid origin token.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguageOriginTrial},
      {network::features::kReduceAcceptLanguage});
  {
    response.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\r\n");
    response.headers->SetHeader("Origin-Trial", kInvalidOriginToken);
    reduce_language_utils.RemoveOriginTrialReducedAcceptLanguage(
        actual_persisted_language.value(), origin, &response, root);
    actual_persisted_language = delegate.GetReducedLanguage(origin);
    EXPECT_EQ(absl::nullopt, actual_persisted_language);
  }
}

TEST_F(AcceptLanguageUtilsTests, ValidateOriginTrial) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguageOriginTrial}, {});

  GURL first_party_url = GURL(kFirstPartyUrl);
  GURL third_party_url = GURL(kThirdPartyOriginUrl);

  {
    SetOriginTrialTokenHeader(kFirstPartyOriginToken);
    EXPECT_TRUE(
        ReduceAcceptLanguageUtils::IsReduceAcceptLanguageEnabledForOrigin(
            url::Origin::Create(first_party_url), response_headers()));
  }

  {
    SetOriginTrialTokenHeader(kThirdPartyOriginToken);
    EXPECT_FALSE(
        ReduceAcceptLanguageUtils::IsReduceAcceptLanguageEnabledForOrigin(
            url::Origin::Create(third_party_url), response_headers()));
  }

  {
    SetOriginTrialTokenHeader(kInvalidOriginToken);
    EXPECT_FALSE(
        ReduceAcceptLanguageUtils::IsReduceAcceptLanguageEnabledForOrigin(
            url::Origin::Create(first_party_url), response_headers()));
  }

  // Expect false when kReduceAcceptLanguageOriginTrial is disable.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {}, {network::features::kReduceAcceptLanguageOriginTrial});
  SetOriginTrialTokenHeader(kFirstPartyOriginToken);
  EXPECT_FALSE(
      ReduceAcceptLanguageUtils::IsReduceAcceptLanguageEnabledForOrigin(
          url::Origin::Create(first_party_url), response_headers()));
}

TEST_F(AcceptLanguageUtilsTests, ThrottleProcessResponse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguageOriginTrial},
      {network::features::kReduceAcceptLanguage});

  MockReduceAcceptLanguageControllerDelegate delegate =
      MockReduceAcceptLanguageControllerDelegate("en,zh", true);
  ReduceAcceptLanguageThrottle throttle =
      ReduceAcceptLanguageThrottle(delegate);

  GURL request_url = GURL("https://mysite.com:4444");
  std::string language = "en";

  network::ResourceRequest request;
  request.url = request_url;
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage, language);
  request.headers = headers;
  bool defer = false;
  throttle.WillStartRequest(&request, &defer);

  delegate.PersistReducedLanguage(url::Origin::Create(request_url), language);

  network::mojom::URLResponseHead response_head;
  response_head.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // Early returns without the variants header.
  {
    throttle.BeforeWillProcessResponse(request_url, response_head, &defer);
    absl::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), language);
  }

  // Early returns with service worker.
  response_head.parsed_headers = network::mojom::ParsedHeaders::New();
  response_head.parsed_headers->content_language =
      network::ParseContentLanguages("en");
  response_head.parsed_headers->variants_headers =
      network::ParseVariantsHeaders("accept-language=(en zh)");
  {
    response_head.did_service_worker_navigation_preload = true;
    throttle.BeforeWillProcessResponse(request_url, response_head, &defer);

    absl::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), language);
  }

  // With valid token should not clear persist language.
  {
    std::string raw_headers = "HTTP/1.1 200 OK\r\n";
    base::StrAppend(&raw_headers,
                    {"Origin-Trial: ", kFirstPartyOriginToken, "\r\n"});
    response_head.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
    throttle.BeforeWillProcessResponse(request_url, response_head, &defer);

    absl::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), language);
  }

  // Without valid origin trial token.
  {
    response_head.did_service_worker_navigation_preload = false;
    throttle.BeforeWillProcessResponse(request_url, response_head, &defer);

    absl::optional<std::string> persist_language =
        delegate.GetReducedLanguage(url::Origin::Create(request_url));
    EXPECT_EQ(persist_language.value(), language);
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

  EXPECT_EQ(ReduceAcceptLanguageUtils::Create(browser_context()),
            absl::nullopt);

  // Expect return a valid instance.
  browser_context()->SetReduceAcceptLanguageControllerDelegate(
      std::make_unique<MockReduceAcceptLanguageControllerDelegate>("en,zh"));
  EXPECT_NE(ReduceAcceptLanguageUtils::Create(browser_context()),
            absl::nullopt);

  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {}, {network::features::kReduceAcceptLanguage,
           network::features::kReduceAcceptLanguageOriginTrial});
  // Feature reset should expect no instance returns
  EXPECT_EQ(ReduceAcceptLanguageUtils::Create(browser_context()),
            absl::nullopt);

  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguageOriginTrial}, {});
  // Expect return a valid instance.
  browser_context()->SetReduceAcceptLanguageControllerDelegate(
      std::make_unique<MockReduceAcceptLanguageControllerDelegate>("en,zh"));
  EXPECT_NE(ReduceAcceptLanguageUtils::Create(browser_context()),
            absl::nullopt);
}

}  // namespace content
