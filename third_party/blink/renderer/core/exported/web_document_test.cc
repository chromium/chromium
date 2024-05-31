// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_document.h"

#include <algorithm>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/web/web_origin_trials.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using blink::frame_test_helpers::WebViewHelper;
using blink::url_test_helpers::ToKURL;

const char kDefaultOrigin[] = "https://example.test/";
const char kOriginTrialDummyFilePath[] = "origin-trial-dummy.html";
const char kNoOriginTrialDummyFilePath[] = "simple_div.html";

class WebDocumentTest : public testing::Test {
 protected:
  static void SetUpTestSuite();

  void LoadURL(const std::string& url);
  Document* TopDocument() const;
  WebDocument TopWebDocument() const;

  test::TaskEnvironment task_environment_;
  WebViewHelper web_view_helper_;
};

void WebDocumentTest::SetUpTestSuite() {
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(std::string(kDefaultOrigin) + kNoOriginTrialDummyFilePath),
      test::CoreTestDataPath(kNoOriginTrialDummyFilePath));
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(std::string(kDefaultOrigin) + kOriginTrialDummyFilePath),
      test::CoreTestDataPath(kOriginTrialDummyFilePath));
}

void WebDocumentTest::LoadURL(const std::string& url) {
  web_view_helper_.InitializeAndLoad(url);
}

Document* WebDocumentTest::TopDocument() const {
  return To<LocalFrame>(web_view_helper_.GetWebView()->GetPage()->MainFrame())
      ->GetDocument();
}

WebDocument WebDocumentTest::TopWebDocument() const {
  return web_view_helper_.LocalMainFrame()->GetDocument();
}

TEST_F(WebDocumentTest, InsertAndRemoveStyleSheet) {
  LoadURL("about:blank");

  WebDocument web_doc = TopWebDocument();
  Document* core_doc = TopDocument();

  unsigned start_count = core_doc->GetStyleEngine().StyleForElementCount();

  WebStyleSheetKey style_sheet_key =
      web_doc.InsertStyleSheet("body { color: green }");

  // Check insertStyleSheet did not cause a synchronous style recalc.
  unsigned element_count =
      core_doc->GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  HTMLElement* body_element = core_doc->body();
  DCHECK(body_element);

  const ComputedStyle& style_before_insertion =
      body_element->ComputedStyleRef();

  // Inserted style sheet not yet applied.
  ASSERT_EQ(Color(0, 0, 0), style_before_insertion.VisitedDependentColor(
                                GetCSSPropertyColor()));

  // Apply inserted style sheet.
  core_doc->UpdateStyleAndLayoutTree();

  const ComputedStyle& style_after_insertion = body_element->ComputedStyleRef();

  // Inserted style sheet applied.
  ASSERT_EQ(Color(0, 128, 0),
            style_after_insertion.VisitedDependentColor(GetCSSPropertyColor()));

  start_count = core_doc->GetStyleEngine().StyleForElementCount();

  // Check RemoveInsertedStyleSheet did not cause a synchronous style recalc.
  web_doc.RemoveInsertedStyleSheet(style_sheet_key);
  element_count =
      core_doc->GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  const ComputedStyle& style_before_removing = body_element->ComputedStyleRef();

  // Removed style sheet not yet applied.
  ASSERT_EQ(Color(0, 128, 0),
            style_before_removing.VisitedDependentColor(GetCSSPropertyColor()));

  // Apply removed style sheet.
  core_doc->UpdateStyleAndLayoutTree();

  const ComputedStyle& style_after_removing = body_element->ComputedStyleRef();
  ASSERT_EQ(Color(0, 0, 0),
            style_after_removing.VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(WebDocumentTest, OriginTrialDisabled) {
  blink::ScopedTestOriginTrialPolicy policy;

  // Load a document with no origin trial token.
  LoadURL(std::string(kDefaultOrigin) + kNoOriginTrialDummyFilePath);
  WebDocument web_doc = TopWebDocument();
  EXPECT_FALSE(WebOriginTrials::isTrialEnabled(&web_doc, "Frobulate"));
}

TEST_F(WebDocumentTest, OriginTrialEnabled) {
  blink::ScopedTestOriginTrialPolicy policy;
  // Load a document with a valid origin trial token for the test trial.
  LoadURL(std::string(kDefaultOrigin) + kOriginTrialDummyFilePath);
  WebDocument web_doc = TopWebDocument();
  EXPECT_TRUE(WebOriginTrials::isTrialEnabled(&web_doc, "Frobulate"));
  // Ensure that other trials are not also enabled
  EXPECT_FALSE(WebOriginTrials::isTrialEnabled(&web_doc, "NotATrial"));
}

namespace {

const char* g_base_url_origin_a = "http://example.test:0/";
const char* g_base_url_origin_sub_a = "http://subdomain.example.test:0/";
const char* g_base_url_origin_secure_a = "https://example.test:0/";
const char* g_base_url_origin_b = "http://not-example.test:0/";
const char* g_empty_file = "first_party/empty.html";
const char* g_nested_data = "first_party/nested-data.html";
const char* g_nested_origin_a = "first_party/nested-originA.html";
const char* g_nested_origin_sub_a = "first_party/nested-originSubA.html";
const char* g_nested_origin_secure_a = "first_party/nested-originSecureA.html";
const char* g_nested_origin_a_in_origin_a =
    "first_party/nested-originA-in-originA.html";
const char* g_nested_origin_a_in_origin_b =
    "first_party/nested-originA-in-originB.html";
const char* g_nested_origin_b = "first_party/nested-originB.html";
const char* g_nested_origin_b_in_origin_a =
    "first_party/nested-originB-in-originA.html";
const char* g_nested_origin_b_in_origin_b =
    "first_party/nested-originB-in-originB.html";
const char* g_nested_src_doc = "first_party/nested-srcdoc.html";

KURL ToFile(const char* file) {
  return ToKURL(std::string("file:///") + file);
}

KURL ToOriginA(const char* file) {
  return ToKURL(std::string(g_base_url_origin_a) + file);
}

KURL ToOriginSubA(const char* file) {
  return ToKURL(std::string(g_base_url_origin_sub_a) + file);
}

KURL ToOriginSecureA(const char* file) {
  return ToKURL(std::string(g_base_url_origin_secure_a) + file);
}

KURL ToOriginB(const char* file) {
  return ToKURL(std::string(g_base_url_origin_b) + file);
}

void RegisterMockedURLLoad(const KURL& url, const char* path) {
  url_test_helpers::RegisterMockedURLLoad(url, test::CoreTestDataPath(path));
}

}  // anonymous namespace

class WebDocumentFirstPartyTest : public WebDocumentTest {
 public:
  static void SetUpTestSuite();

 protected:
  void Load(const char*);
  Document* NestedDocument() const;
  Document* NestedNestedDocument() const;
};

void WebDocumentFirstPartyTest::SetUpTestSuite() {
  RegisterMockedURLLoad(ToOriginA(g_empty_file), g_empty_file);
  RegisterMockedURLLoad(ToOriginA(g_nested_data), g_nested_data);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_a), g_nested_origin_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_sub_a),
                        g_nested_origin_sub_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_secure_a),
                        g_nested_origin_secure_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_a_in_origin_a),
                        g_nested_origin_a_in_origin_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_a_in_origin_b),
                        g_nested_origin_a_in_origin_b);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_b), g_nested_origin_b);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_b_in_origin_a),
                        g_nested_origin_b_in_origin_a);
  RegisterMockedURLLoad(ToOriginA(g_nested_origin_b_in_origin_b),
                        g_nested_origin_b_in_origin_b);
  RegisterMockedURLLoad(ToOriginA(g_nested_src_doc), g_nested_src_doc);
  RegisterMockedURLLoad(ToOriginSubA(g_empty_file), g_empty_file);
  RegisterMockedURLLoad(ToOriginSecureA(g_empty_file), g_empty_file);
  RegisterMockedURLLoad(ToOriginB(g_empty_file), g_empty_file);
  RegisterMockedURLLoad(ToOriginB(g_nested_origin_a), g_nested_origin_a);
  RegisterMockedURLLoad(ToOriginB(g_nested_origin_b), g_nested_origin_b);

  RegisterMockedURLLoad(ToFile(g_nested_origin_a), g_nested_origin_a);
}

void WebDocumentFirstPartyTest::Load(const char* file) {
  web_view_helper_.InitializeAndLoad(std::string(g_base_url_origin_a) + file);
}

Document* WebDocumentFirstPartyTest::NestedDocument() const {
  return To<LocalFrame>(web_view_helper_.GetWebView()
                            ->GetPage()
                            ->MainFrame()
                            ->Tree()
                            .FirstChild())
      ->GetDocument();
}

Document* WebDocumentFirstPartyTest::NestedNestedDocument() const {
  return To<LocalFrame>(web_view_helper_.GetWebView()
                            ->GetPage()
                            ->MainFrame()
                            ->Tree()
                            .FirstChild()
                            ->Tree()
                            .FirstChild())
      ->GetDocument();
}

bool OriginsEqual(const char* path,
                  scoped_refptr<const SecurityOrigin> origin) {
  return SecurityOrigin::Create(ToOriginA(path))
      ->IsSameOriginWith(origin.get());
}

bool SiteForCookiesEqual(const char* path,
                         const net::SiteForCookies& site_for_cookies) {
  KURL ref_url = ToOriginA(path);
  ref_url.SetPort(80);  // url::Origin takes exception with :0.
  return net::SiteForCookies::FromUrl(GURL(ref_url))
      .IsEquivalent(site_for_cookies);
}

TEST_F(WebDocumentFirstPartyTest, Empty) {
  Load(g_empty_file);

  ASSERT_TRUE(
      SiteForCookiesEqual(g_empty_file, TopDocument()->SiteForCookies()));
  ASSERT_TRUE(OriginsEqual(g_empty_file, TopDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, EmptySandbox) {
  web_view_helper_.Initialize();
  WebLocalFrameImpl* frame = web_view_helper_.GetWebView()->MainFrameImpl();
  auto params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(KURL("https://a.com"));
  MockPolicyContainerHost mock_policy_container_host;
  params->policy_container = std::make_unique<blink::WebPolicyContainer>(
      blink::WebPolicyContainerPolicies(),
      mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
  params->policy_container->policies.sandbox_flags =
      network::mojom::blink::WebSandboxFlags::kAll;
  frame->CommitNavigation(std::move(params), nullptr /* extra_data */);
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(frame);

  ASSERT_TRUE(TopDocument()->TopFrameOrigin()->IsOpaque())
      << TopDocument()->TopFrameOrigin()->ToUrlOrigin().GetDebugString();
  ASSERT_TRUE(TopDocument()->SiteForCookies().IsNull());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginA) {
  Load(g_nested_origin_a);

  ASSERT_TRUE(
      SiteForCookiesEqual(g_nested_origin_a, TopDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a,
                                  NestedDocument()->SiteForCookies()));

  ASSERT_TRUE(OriginsEqual(g_nested_origin_a, TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(
      OriginsEqual(g_nested_origin_a, NestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginASchemefulSiteForCookies) {
  Load(g_nested_origin_a);

  // TopDocument is same scheme with itself so expect true.
  ASSERT_TRUE(TopDocument()->SiteForCookies().schemefully_same());
  // NestedDocument is same scheme with TopDocument so expect true.
  ASSERT_TRUE(NestedDocument()->SiteForCookies().schemefully_same());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSubA) {
  Load(g_nested_origin_sub_a);

  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_sub_a,
                                  TopDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_sub_a,
                                  NestedDocument()->SiteForCookies()));

  ASSERT_TRUE(
      OriginsEqual(g_nested_origin_sub_a, TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(
      OriginsEqual(g_nested_origin_sub_a, NestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSecureA) {
  Load(g_nested_origin_secure_a);

  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_secure_a,
                                  TopDocument()->SiteForCookies()));
  // Since NestedDocument is secure, and the parent is insecure, its
  // SiteForCookies will be null and therefore will not match.
  ASSERT_FALSE(SiteForCookiesEqual(g_nested_origin_secure_a,
                                   NestedDocument()->SiteForCookies()));
  // However its site shouldn't be opaque
  ASSERT_FALSE(NestedDocument()->SiteForCookies().site().opaque());

  ASSERT_TRUE(
      OriginsEqual(g_nested_origin_secure_a, TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_secure_a,
                           NestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSecureASchemefulSiteForCookies) {
  Load(g_nested_origin_secure_a);

  // TopDocument is same scheme with itself so expect true.
  ASSERT_TRUE(TopDocument()->SiteForCookies().schemefully_same());

  // Since NestedDocument is secure, and the parent is insecure, the scheme will
  // differ.
  ASSERT_FALSE(NestedDocument()->SiteForCookies().schemefully_same());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginA) {
  Load(g_nested_origin_a_in_origin_a);

  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a_in_origin_a,
                                  TopDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a_in_origin_a,
                                  NestedDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a_in_origin_a,
                                  NestedNestedDocument()->SiteForCookies()));

  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_a,
                           TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_a,
                           NestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginB) {
  Load(g_nested_origin_a_in_origin_b);

  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a_in_origin_b,
                                  TopDocument()->SiteForCookies()));
  ASSERT_TRUE(NestedDocument()->SiteForCookies().IsNull());
  ASSERT_TRUE(NestedNestedDocument()->SiteForCookies().IsNull());

  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_b,
                           TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_b,
                           NestedDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_b,
                           NestedNestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginB) {
  Load(g_nested_origin_b);

  ASSERT_TRUE(
      SiteForCookiesEqual(g_nested_origin_b, TopDocument()->SiteForCookies()));
  ASSERT_TRUE(NestedDocument()->SiteForCookies().IsNull());

  ASSERT_TRUE(OriginsEqual(g_nested_origin_b, TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(
      OriginsEqual(g_nested_origin_b, NestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginA) {
  Load(g_nested_origin_b_in_origin_a);

  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_b_in_origin_a,
                                  TopDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_b_in_origin_a,
                                  NestedDocument()->SiteForCookies()));
  ASSERT_TRUE(NestedNestedDocument()->SiteForCookies().IsNull());

  ASSERT_TRUE(OriginsEqual(g_nested_origin_b_in_origin_a,
                           TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_b_in_origin_a,
                           NestedDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_b_in_origin_a,
                           NestedNestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginB) {
  Load(g_nested_origin_b_in_origin_b);

  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_b_in_origin_b,
                                  TopDocument()->SiteForCookies()));
  ASSERT_TRUE(NestedDocument()->SiteForCookies().IsNull());
  ASSERT_TRUE(NestedNestedDocument()->SiteForCookies().IsNull());

  ASSERT_TRUE(OriginsEqual(g_nested_origin_b_in_origin_b,
                           TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_b_in_origin_b,
                           NestedDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_b_in_origin_b,
                           NestedNestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedSrcdoc) {
  Load(g_nested_src_doc);

  ASSERT_TRUE(
      SiteForCookiesEqual(g_nested_src_doc, TopDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_src_doc,
                                  NestedDocument()->SiteForCookies()));

  ASSERT_TRUE(OriginsEqual(g_nested_src_doc, TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(
      OriginsEqual(g_nested_src_doc, NestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, NestedData) {
  Load(g_nested_data);

  ASSERT_TRUE(
      SiteForCookiesEqual(g_nested_data, TopDocument()->SiteForCookies()));
  ASSERT_TRUE(NestedDocument()->SiteForCookies().IsNull());

  ASSERT_TRUE(OriginsEqual(g_nested_data, TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_data, NestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest,
       NestedOriginAInOriginBWithFirstPartyOverride) {
  Load(g_nested_origin_a_in_origin_b);

#if DCHECK_IS_ON()
  // TODO(crbug.com/1329535): Remove if threaded preload scanner doesn't launch.
  // This is needed because the preload scanner creates a thread when loading a
  // page.
  WTF::SetIsBeforeThreadCreatedForTest();
#endif
  SchemeRegistry::RegisterURLSchemeAsFirstPartyWhenTopLevel("http");

  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a_in_origin_b,
                                  TopDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a_in_origin_b,
                                  NestedDocument()->SiteForCookies()));
  ASSERT_TRUE(SiteForCookiesEqual(g_nested_origin_a_in_origin_b,
                                  NestedNestedDocument()->SiteForCookies()));

  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_b,
                           TopDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_b,
                           NestedDocument()->TopFrameOrigin()));
  ASSERT_TRUE(OriginsEqual(g_nested_origin_a_in_origin_b,
                           NestedNestedDocument()->TopFrameOrigin()));
}

TEST_F(WebDocumentFirstPartyTest, FileScheme) {
  web_view_helper_.InitializeAndLoad(std::string("file:///") +
                                     g_nested_origin_a);

  net::SiteForCookies top_site_for_cookies = TopDocument()->SiteForCookies();
  EXPECT_EQ("file", top_site_for_cookies.scheme());
  EXPECT_EQ("", top_site_for_cookies.registrable_domain());

  // Nested a.com is 3rd-party to file://
  EXPECT_TRUE(NestedDocument()->SiteForCookies().IsNull());
}

}  // namespace blink
