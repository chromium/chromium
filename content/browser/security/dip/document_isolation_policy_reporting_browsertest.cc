// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_cert_verifier_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr char kDipHost[] = "dip.test";
constexpr char kCrossOriginHost[] = "crossorigin.test";

// End-to-end coverage for Document-Isolation-Policy violation reports that are
// delivered over the network to an endpoint declared with the
// `Reporting-Endpoints` header.
//
// The test document deliberately declares *only* `Reporting-Endpoints` (V1
// Reporting) and never the legacy `Report-To` header (V0 Reporting). This
// matters: V1 endpoints are stored keyed on the document's reporting source,
// and `ReportingCacheImpl::GetCandidateEndpointsForDelivery()` silently falls
// back to the V0 origin-scoped endpoints when that lookup misses. Were a V0
// endpoint also configured, a wrong reporting source would still deliver the
// report and the test would pass vacuously. With only V1 configured, a
// mismatched reporting source surfaces as zero delivered reports.
class DocumentIsolationPolicyReportingBrowserTest
    : public CertVerifierBrowserTest {
 public:
  DocumentIsolationPolicyReportingBrowserTest() {
    feature_list_.InitWithFeatures(
        {network::features::kDocumentIsolationPolicy,
         features::kDocumentIsolationPolicyWithoutSiteIsolation},
        {});
  }

 protected:
  // A document served with enforcing DIP and a single document-scoped
  // reporting endpoint named "default".
  GURL DipDocumentUrl() const {
    return embedded_https_test_server().GetURL(kDipHost, "/dip-reporting");
  }

  // A cross-origin resource served *without* `Cross-Origin-Resource-Policy`.
  // Fetching it from a DIP document is a CORP violation.
  GURL NoCorpResourceUrl() const {
    return embedded_https_test_server().GetURL(kCrossOriginHost, "/echo");
  }

  net::test_server::ControllableHttpResponse& collector() {
    return *collector_;
  }

 private:
  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");

    collector_ = std::make_unique<net::test_server::ControllableHttpResponse>(
        &embedded_https_test_server(), "/upload");
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/dip-reporting",
        base::BindRepeating(
            &DocumentIsolationPolicyReportingBrowserTest::DipDocumentHandler,
            base::Unretained(this))));
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  // The collector is same-origin with the DIP document so that uploads are not
  // preceded by a CORS preflight.
  GURL CollectorUrl() const {
    return embedded_https_test_server().GetURL(kDipHost, "/upload");
  }

  std::unique_ptr<net::test_server::HttpResponse> DipDocumentHandler(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->AddCustomHeader(
        "Document-Isolation-Policy",
        "isolate-and-require-corp; report-to=\"default\"");
    response->AddCustomHeader("Reporting-Endpoints",
                              "default=\"" + CollectorUrl().spec() + "\"");
    response->set_content_type("text/html");
    response->set_content("<!doctype html><title>dip</title>");
    return response;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> collector_;
};

// A DIP document that configures a `Reporting-Endpoints` collector must receive
// CORP violation reports over the network.
//
// This regression-tests that `DocumentIsolationPolicyReporter` is handed the
// committed document's reporting source. The reporter is created during
// navigation, before the reporting source is known, so it is seeded with the
// frame token as a placeholder; `RenderFrameHostImpl` overwrites it at commit
// time. Without that call the report is queued against the frame token, the
// document-scoped endpoint lookup misses, and nothing is ever delivered.
IN_PROC_BROWSER_TEST_F(DocumentIsolationPolicyReportingBrowserTest,
                       CorpViolationReportDeliveredToDocumentEndpoint) {
  ASSERT_TRUE(NavigateToURL(shell(), DipDocumentUrl()));

  // The cross-origin resource carries no `Cross-Origin-Resource-Policy`, so
  // enforcing DIP must block it. `no-cors` keeps CORS from rejecting the
  // request before the CORP check runs.
  EXPECT_EQ("error", EvalJs(shell(), JsReplace(R"(
      fetch($1, {mode: 'no-cors'}).then(() => 'ok', () => 'error');
  )",
                                               NoCorpResourceUrl())));

  collector().WaitForRequest();
  ASSERT_EQ(net::test_server::METHOD_POST, collector().http_request()->method);
  const std::string& upload = collector().http_request()->content;
  collector().Send(net::HTTP_NO_CONTENT);
  collector().Done();

  std::optional<base::ListValue> parsed =
      base::JSONReader::ReadList(upload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed.has_value()) << "upload body: " << upload;
  ASSERT_EQ(1u, parsed->size());

  const base::DictValue* report = (*parsed)[0].GetIfDict();
  ASSERT_TRUE(report);
  EXPECT_THAT(report->FindString("type"),
              ::testing::Pointee(std::string("dip")));
  EXPECT_THAT(report->FindString("url"),
              ::testing::Pointee(DipDocumentUrl().spec()));

  const base::DictValue* body = report->FindDict("body");
  ASSERT_TRUE(body);
  EXPECT_THAT(body->FindString("type"),
              ::testing::Pointee(std::string("corp")));
  EXPECT_THAT(body->FindString("disposition"),
              ::testing::Pointee(std::string("enforce")));
  EXPECT_THAT(body->FindString("blockedURL"),
              ::testing::Pointee(NoCorpResourceUrl().spec()));
}

}  // namespace
}  // namespace content
