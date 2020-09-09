// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ancestor_throttle.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_navigation_url_loader.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using HeaderDisposition = AncestorThrottle::HeaderDisposition;

net::HttpResponseHeaders* GetAncestorHeaders(const char* xfo, const char* csp) {
  std::string header_string("HTTP/1.1 200 OK\nX-Frame-Options: ");
  header_string += xfo;
  if (csp != nullptr) {
    header_string += "\nContent-Security-Policy: ";
    header_string += csp;
  }
  header_string += "\n\n";
  std::replace(header_string.begin(), header_string.end(), '\n', '\0');
  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(header_string);
  EXPECT_TRUE(headers->HasHeader("X-Frame-Options"));
  if (csp != nullptr)
    EXPECT_TRUE(headers->HasHeader("Content-Security-Policy"));
  return headers;
}

network::mojom::ContentSecurityPolicyPtr ParsePolicy(
    const std::string& policy) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  headers->SetHeader("Content-Security-Policy", policy);
  std::vector<network::mojom::ContentSecurityPolicyPtr> policies;
  network::AddContentSecurityPolicyFromHeaders(
      *headers, GURL("https://example.com/"), &policies);
  return std::move(policies[0]);
}

}  // namespace

// AncestorThrottleTest
// -------------------------------------------------------------

class AncestorThrottleTest : public testing::Test {};

TEST_F(AncestorThrottleTest, ParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* value;
  } cases[] = {
      // Basic keywords
      {"DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL"},

      // Repeated keywords
      {"DENY,DENY", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL,ALLOWALL", HeaderDisposition::ALLOWALL, "ALLOWALL, ALLOWALL"},

      // Case-insensitive
      {"deNy", HeaderDisposition::DENY, "deNy"},
      {"sAmEorIgIn", HeaderDisposition::SAMEORIGIN, "sAmEorIgIn"},
      {"AlLOWaLL", HeaderDisposition::ALLOWALL, "AlLOWaLL"},

      // Trim whitespace
      {" DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {" ALLOWALL ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {"   DENY", HeaderDisposition::DENY, "DENY"},
      {"SAMEORIGIN   ", HeaderDisposition::SAMEORIGIN, "SAMEORIGIN"},
      {"   ALLOWALL   ", HeaderDisposition::ALLOWALL, "ALLOWALL"},
      {" DENY , DENY ", HeaderDisposition::DENY, "DENY, DENY"},
      {"SAMEORIGIN,  SAMEORIGIN", HeaderDisposition::SAMEORIGIN,
       "SAMEORIGIN, SAMEORIGIN"},
      {"ALLOWALL  ,ALLOWALL", HeaderDisposition::ALLOWALL,
       "ALLOWALL, ALLOWALL"},
  };

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseXFrameOptionsHeader(headers.get(), &header_value));
    EXPECT_EQ(test.value, header_value);
  }
}

TEST_F(AncestorThrottleTest, ErrorsParsingXFrameOptions) {
  struct TestCase {
    const char* header;
    AncestorThrottle::HeaderDisposition expected;
    const char* failure;
  } cases[] = {
      // Empty == Invalid.
      {"", HeaderDisposition::INVALID, ""},

      // Invalid
      {"INVALID", HeaderDisposition::INVALID, "INVALID"},
      {"INVALID DENY", HeaderDisposition::INVALID, "INVALID DENY"},
      {"DENY DENY", HeaderDisposition::INVALID, "DENY DENY"},
      {"DE NY", HeaderDisposition::INVALID, "DE NY"},

      // Conflicts
      {"INVALID,DENY", HeaderDisposition::CONFLICT, "INVALID, DENY"},
      {"DENY,ALLOWALL", HeaderDisposition::CONFLICT, "DENY, ALLOWALL"},
      {"SAMEORIGIN,DENY", HeaderDisposition::CONFLICT, "SAMEORIGIN, DENY"},
      {"ALLOWALL,SAMEORIGIN", HeaderDisposition::CONFLICT,
       "ALLOWALL, SAMEORIGIN"},
      {"DENY,  SAMEORIGIN", HeaderDisposition::CONFLICT, "DENY, SAMEORIGIN"}};

  AncestorThrottle throttle(nullptr);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.header);
    scoped_refptr<net::HttpResponseHeaders> headers =
        GetAncestorHeaders(test.header, nullptr);
    std::string header_value;
    EXPECT_EQ(test.expected,
              throttle.ParseXFrameOptionsHeader(headers.get(), &header_value));
    EXPECT_EQ(test.failure, header_value);
  }
}

TEST_F(AncestorThrottleTest, AllowsBlanketEnforcementOfRequiredCSP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kOutOfBlinkCSPEE);

  struct TestCase {
    const char* name;
    const char* request_origin;
    const char* response_origin;
    const char* allow_csp_from;
    bool expected_result;
  } cases[] = {
      {
          "About scheme allows",
          "http://example.com",
          "about://me",
          nullptr,
          true,
      },
      {
          "File scheme allows",
          "http://example.com",
          "file://me",
          nullptr,
          true,
      },
      {
          "Data scheme allows",
          "http://example.com",
          "data://me",
          nullptr,
          true,
      },
      {
          "Filesystem scheme allows",
          "http://example.com",
          "filesystem://me",
          nullptr,
          true,
      },
      {
          "Blob scheme allows",
          "http://example.com",
          "blob://me",
          nullptr,
          true,
      },
      {
          "Same origin allows",
          "http://example.com",
          "http://example.com",
          nullptr,
          true,
      },
      {
          "Same origin allows independently of header",
          "http://example.com",
          "http://example.com",
          "http://not-example.com",
          true,
      },
      {
          "Different origin does not allow",
          "http://example.com",
          "http://not.example.com",
          nullptr,
          false,
      },
      {
          "Different origin with right header allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com",
          true,
      },
      {
          "Different origin with right header 2 allows",
          "http://example.com",
          "http://not-example.com",
          "http://example.com/",
          true,
      },
      {
          "Different origin with wrong header does not allow",
          "http://example.com",
          "http://not-example.com",
          "http://not-example.com",
          false,
      },
      {
          "Wildcard header allows",
          "http://example.com",
          "http://not-example.com",
          "*",
          true,
      },
      {
          "Malformed header does not allow",
          "http://example.com",
          "http://not-example.com",
          "*; http://example.com",
          false,
      },
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (test.allow_csp_from)
      headers->AddHeader("allow-csp-from", test.allow_csp_from);
    auto allow_csp_from = network::ParseAllowCSPFromHeader(*headers);

    bool actual = AncestorThrottle::AllowsBlanketEnforcementOfRequiredCSP(
        url::Origin::Create(GURL(test.request_origin)),
        GURL(test.response_origin), allow_csp_from);
    EXPECT_EQ(test.expected_result, actual);
  }
}

using AncestorThrottleNavigationTest = RenderViewHostTestHarness;

TEST_F(AncestorThrottleNavigationTest,
       WillStartRequestAddsSecRequiredCSPHeader) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kOutOfBlinkCSPEE);

  // Create a frame tree with different 'csp' attributes according to the
  // following graph:
  //
  // FRAME NAME                    | 'csp' attribute
  // ------------------------------|-------------------------------------
  // main_frame                    | (none)
  //  ├─child_with_csp             | script-src 'none'
  //  │  ├─grandchild_same_csp     | script-src 'none'
  //  │  ├─grandchild_no_csp       | (none)
  //  │  │ └─grandgrandchild       | (none)
  //  │  ├─grandchild_invalid_csp  | report-to group
  //  │  └─grandchild_invalid_csp2 | script-src 'none'; invalid-directive
  //  └─sibling                    | (none)
  //
  // Test that the required CSP of every frame is computed/inherited correctly
  // and that the Sec-Required-CSP header is set.

  auto test = [](TestRenderFrameHost* frame, std::string csp_attr,
                 std::string expect_csp) {
    SCOPED_TRACE(frame->GetFrameName());

    if (!csp_attr.empty())
      frame->frame_tree_node()->set_csp_attribute(ParsePolicy(csp_attr));

    std::unique_ptr<NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            // Chrome blocks a frame navigating to a URL if more than one of its
            // ancestors have the same URL. Use a different URL every time, to
            // avoid blocking navigation of the grandchild frame.
            GURL("https://www.example.com/" + frame->GetFrameName()), frame);
    simulator->Start();
    NavigationRequest* request =
        NavigationRequest::From(simulator->GetNavigationHandle());
    std::string header_value;
    bool found = request->GetRequestHeaders().GetHeader("sec-required-csp",
                                                        &header_value);
    if (!expect_csp.empty()) {
      EXPECT_TRUE(found);
      EXPECT_EQ(expect_csp, header_value);
    } else {
      EXPECT_FALSE(found);
    }

    // Complete the navigation so that the required csp is stored in the
    // RenderFrameHost, so that when we will add children to this frame they
    // will be able to get the parent's required csp (and hence also test that
    // the whole logic works).
    auto response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    response_headers->SetHeader("Allow-CSP-From", "*");
    simulator->SetResponseHeaders(response_headers);
    simulator->Commit();
  };

  auto* main_frame = static_cast<TestRenderFrameHost*>(main_rfh());
  test(main_frame, "", "");

  auto* child_with_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_frame)
          ->AppendChild("child_with_csp"));
  test(child_with_csp, "script-src 'none'", "script-src 'none'");

  auto* grandchild_same_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_same_csp"));
  test(grandchild_same_csp, "script-src 'none'", "script-src 'none'");

  auto* grandchild_no_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_no_csp"));
  test(grandchild_no_csp, "", "script-src 'none'");

  auto* grandgrandchild = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(grandchild_no_csp)
          ->AppendChild("grandgrandchild"));
  test(grandgrandchild, "", "script-src 'none'");

  auto* grandchild_invalid_csp = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_invalid_csp"));
  test(grandchild_invalid_csp, "report-to group", "script-src 'none'");

  auto* grandchild_invalid_csp2 = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(child_with_csp)
          ->AppendChild("grandchild_invalid_csp2"));
  test(grandchild_invalid_csp2, "script-src 'none'; invalid-directive",
       "script-src 'none'");

  auto* sibling = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_frame)->AppendChild("sibling"));
  test(sibling, "", "");
}

TEST_F(AncestorThrottleNavigationTest, EvaluateCSPEmbeddedEnforcement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kOutOfBlinkCSPEE);

  // We need one initial navigation to set up everything.
  NavigateAndCommit(GURL("https://www.example.org"));

  auto* main_frame = static_cast<TestRenderFrameHost*>(main_rfh());

  struct TestCase {
    const char* name;
    const char* required_csp;
    const char* frame_url;
    const char* allow_csp_from;
    const char* returned_csp;
    bool expect_allow;
  } cases[] = {
      {
          "No required csp",
          nullptr,
          "https://www.not-example.org",
          nullptr,
          nullptr,
          true,
      },
      {
          "Required csp - Same origin",
          "script-src 'none'",
          "https://www.example.org",
          nullptr,
          nullptr,
          true,
      },
      {
          "Required csp - Cross origin",
          "script-src 'none'",
          "https://www.not-example.org",
          nullptr,
          nullptr,
          false,
      },
      {
          "Required csp - Cross origin with Allow-CSP-From",
          "script-src 'none'",
          "https://www.not-example.org",
          "*",
          nullptr,
          true,
      },
      {
          "Required csp - Cross origin with wrong Allow-CSP-From",
          "script-src 'none'",
          "https://www.not-example.org",
          "https://www.another-example.org",
          nullptr,
          false,
      },
      {
          "Required csp - Cross origin with non-subsuming CSPs",
          "script-src 'none'",
          "https://www.not-example.org",
          nullptr,
          "style-src 'none'",
          false,
      },
      {
          "Required csp - Cross origin with subsuming CSPs",
          "script-src 'none'",
          "https://www.not-example.org",
          nullptr,
          "script-src 'none'",
          true,
      },
      {
          "Required csp - Cross origin with wrong Allow-CSP-From but subsuming "
          "CSPs",
          "script-src 'none'",
          "https://www.not-example.org",
          "https://www.another-example.org",
          "script-src 'none'",
          true,
      },
  };

  for (auto test : cases) {
    SCOPED_TRACE(test.name);
    auto* frame = static_cast<TestRenderFrameHost*>(
        content::RenderFrameHostTester::For(main_frame)
            ->AppendChild(test.name));

    if (test.required_csp) {
      frame->frame_tree_node()->set_csp_attribute(
          ParsePolicy(test.required_csp));
    }

    std::unique_ptr<NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            GURL(test.frame_url), frame);

    auto response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (test.allow_csp_from)
      response_headers->SetHeader("Allow-CSP-From", test.allow_csp_from);
    if (test.returned_csp)
      response_headers->SetHeader("Content-Security-Policy", test.returned_csp);

    simulator->SetResponseHeaders(response_headers);
    simulator->ReadyToCommit();

    if (test.expect_allow) {
      EXPECT_EQ(NavigationThrottle::PROCEED,
                simulator->GetLastThrottleCheckResult());
    } else {
      EXPECT_EQ(NavigationThrottle::BLOCK_RESPONSE,
                simulator->GetLastThrottleCheckResult());
    }
  }
}

}  // namespace content
