// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the ResponseAnalyzerTests (which test the response
// analyzer's behavior in several parameterized test scenarios) and at the end
// includes the CrossOriginReadBlockingTests, which are more typical unittests.

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/corb/corb_impl.h"
#include "services/network/public/cpp/corb/orb_impl.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::StringPiece;
using CorbResponseAnalyzer =
    network::corb::CrossOriginReadBlocking::CorbResponseAnalyzer;
using CrossOriginProtectionDecision =
    CorbResponseAnalyzer::CrossOriginProtectionDecision;
using MimeType = network::corb::CrossOriginReadBlocking::MimeType;
using MimeTypeBucket = CorbResponseAnalyzer::MimeTypeBucket;
using SniffingResult = network::corb::CrossOriginReadBlocking::SniffingResult;

namespace network::corb {

namespace {

// CORB's verdict on a given scenario. kAllowBecauseOutOfData occurs when one of
// the sniffers still desires more data but the response has run out, or
// net::kMaxBytesToSniff has been reached.
enum class Verdict {
  kAllow,
  kBlock,
  kAllowBecauseOutOfData,
};

constexpr int kVerdictPacketForHeadersBasedVerdict = -1;
constexpr int kVerdictPacketForInconclusiveSniffing = 99999;

// This struct is used to describe each test case in this file.  It's passed as
// a test parameter to each TEST_P test.
struct TestScenario {
  // Attributes to make test failure messages useful.
  const char* description;
  int source_line;

  // Attributes of the HTTP Request.
  const char* target_url;
  const char* initiator_origin;

  // Attributes of the HTTP response.
  const char* response_headers;
  const char* response_content_type;
  MimeType canonical_mime_type;
  // Categorizes the MIME type as public, (CORB) protected or other.
  MimeTypeBucket mime_type_bucket;
  // |packets| specifies the response data which may arrive over the course of
  // several writes.
  std::initializer_list<const char*> packets;

  std::string data() const {
    std::string data;
    for (const char* packet : packets) {
      data += packet;
    }
    return data;
  }

  // Whether the resource should seem sensitive (either through the CORS
  // heuristic or the Cache heuristic). This is used for testing that CORB would
  // have protected the resource, were it requested cross-origin.
  bool resource_is_sensitive;
  // Whether we expect CORB to protect the resource on a cross-origin request.
  // Note this value is not checked if resource_is_sensitive is false. Also note
  // that the protection decision may be kBlockedAfterSniffing /
  // kAllowedAfterSniffing despite a nosniff header as we still sniff for the
  // javascript parser breaker and json in these cases.
  CrossOriginProtectionDecision protection_decision;

  // Expected result.
  Verdict verdict;
  // The packet number during which the verdict is decided.
  // kVerdictPacketForHeadersBasedVerdict means that the verdict can be decided
  // before the first packet's data is available. |packets.size()| means that
  // the verdict is decided during the end-of-stream call.
  int verdict_packet;
};

// Stream operator to let GetParam() print a useful result if any tests fail.
::std::ostream& operator<<(::std::ostream& os, const TestScenario& scenario) {
  std::string verdict;
  switch (scenario.verdict) {
    case Verdict::kAllow:
      verdict = "Verdict::kAllow";
      break;
    case Verdict::kBlock:
      verdict = "Verdict::kBlock";
      break;
    case Verdict::kAllowBecauseOutOfData:
      verdict = "Verdict::kAllowBecauseOutOfData";
      break;
  }

  std::string response_headers_formatted;
  base::ReplaceChars(scenario.response_headers, "\n",
                     "\n                          ",
                     &response_headers_formatted);

  std::string mime_type_bucket;
  switch (scenario.mime_type_bucket) {
    case MimeTypeBucket::kProtected:
      mime_type_bucket = "MimeTypeBucket::kProtected";
      break;
    case MimeTypeBucket::kPublic:
      mime_type_bucket = "MimeTypeBucket::kPublic";
      break;
    case MimeTypeBucket::kOther:
      mime_type_bucket = "MimeTypeBucket::kOther";
      break;
  }

  std::string packets = "{";
  for (std::string packet : scenario.packets) {
    base::ReplaceChars(packet, "\\", "\\\\", &packet);
    base::ReplaceChars(packet, "\"", "\\\"", &packet);
    base::ReplaceChars(packet, "\n", "\\n", &packet);
    base::ReplaceChars(packet, "\t", "\\t", &packet);
    base::ReplaceChars(packet, "\r", "\\r", &packet);
    if (packets.length() > 1)
      packets += ", ";
    packets += "\"";
    packets += packet;
    packets += "\"";
  }
  packets += "}";

  std::string protection_decision;
  switch (scenario.protection_decision) {
    case CrossOriginProtectionDecision::kAllow:
      protection_decision = "CrossOriginProtectionDecision::kAllow";
      break;
    case CrossOriginProtectionDecision::kBlock:
      protection_decision = "CrossOriginProtectionDecision::kBlock";
      break;
    case CrossOriginProtectionDecision::kNeedToSniffMore:
      protection_decision = "CrossOriginProtectionDecision::kNeedToSniffMore";
      break;
    case CrossOriginProtectionDecision::kAllowedAfterSniffing:
      protection_decision =
          "CrossOriginProtectionDecision::kAllowedAfterSniffing";
      break;
    case CrossOriginProtectionDecision::kBlockedAfterSniffing:
      protection_decision =
          "CrossOriginProtectionDecision::kBlockedAfterSniffing";
      break;
  }

  return os << "\n  description           = " << scenario.description
            << "\n  source_line           = " << scenario.source_line
            << "\n  target_url            = " << scenario.target_url
            << "\n  initiator_origin      = " << scenario.initiator_origin
            << "\n  response_headers      = " << response_headers_formatted
            << "\n  response_content_type = " << scenario.response_content_type
            << "\n  canonical_mime_type   = " << scenario.canonical_mime_type
            << "\n  mime_type_bucket      = " << mime_type_bucket
            << "\n  packets               = " << packets
            << "\n  resource_is_sensitive = "
            << (scenario.resource_is_sensitive ? "true" : "false")
            << "\n  protection_decision   = " << protection_decision
            << "\n  verdict               = " << verdict
            << "\n  verdict_packet        = " << scenario.verdict_packet;
}

// An HTML response with an HTML comment that's longer than the sniffing
// threshold. We don't sniff past net::kMaxBytesToSniff, so these are not
// protected
const char kHTMLWithTooLongComment[] =
    "<!--.............................................................72 chars"
    "................................................................144 chars"
    "................................................................216 chars"
    "................................................................288 chars"
    "................................................................360 chars"
    "................................................................432 chars"
    "................................................................504 chars"
    "................................................................576 chars"
    "................................................................648 chars"
    "................................................................720 chars"
    "................................................................792 chars"
    "................................................................864 chars"
    "................................................................936 chars"
    "...............................................................1008 chars"
    "...............................................................1080 chars"
    "--><html><head>";

// A set of test cases that verify CrossSiteDocumentResourceHandler correctly
// classifies network responses as allowed or blocked. These TestScenarios are
// passed to the TEST_P tests below as test parameters.
const TestScenario kScenarios[] = {

    // Allowed responses (without sniffing):
    {
        "Allowed: Same-origin XHR to HTML",
        __LINE__,
        "http://www.a.com/resource.html",           // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed: Same-origin JSON with parser breaker and HTML mime type",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {")]}',\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed: Same-origin JSON with parser breaker and JSON mime type",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/json",                       // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {")]}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed: Cross-site script without parser breaker",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "application/javascript",          // response_content_type
        MimeType::kOthers,                 // canonical_mime_type
        MimeTypeBucket::kPublic,           // mime_type_bucket
        {"var x=3;"},                      // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to HTML with CORS for origin",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to XML with CORS for any",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: *",  // response_headers
        "application/rss+xml",             // response_content_type
        MimeType::kXml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"},  // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to JSON with CORS for null",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: null",    // response_headers
        "text/json",                            // response_content_type
        MimeType::kJson,                        // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {"{\"x\" : 3}"},                        // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        // This case won't be reached in practice today, because CORB is only
        // used by certain URLLoaderFactories (e.g. in the NetworkService, when
        // handling http(s) URLs) and is not used for ftp://... URLs.
        "Blocked: Cross-site XHR to HTML over FTP",
        __LINE__,
        "ftp://www.b.com/resource.html",            // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,      // protection_decision
        Verdict::kBlock,                            // verdict
        0,                                          // verdict_packet
    },
    {
        // This case won't be reached in practice today, because CORB is only
        // used by certain URLLoaderFactories (e.g. in the NetworkService, when
        // handling http(s) URLs) and is not used for file://... URLs.
        "Blocked: Cross-site XHR to HTML from file://",
        __LINE__,
        "file:///foo/resource.html",                // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,      // protection_decision
        Verdict::kBlock,                            // verdict
        0,                                          // verdict_packet
    },
    {
        // Blocked. (Simulating a behavior of a compromised renderer that only
        // pretends to be hosting PDF).
        "Blocked: Cross-site fetch HTML from Flash without CORS",
        __LINE__,
        "http://www.b.com/plugin.html",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Allowed: Cross-site fetch HTML from NaCl with CORS response",
        __LINE__,
        "http://www.b.com/plugin.html",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // first_chunk
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Allowed: JSON object + CORS with parser-breaker labeled as JavaScript",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: *",       // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {")]}'\n[true, false]"},                // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Blocked: JSON object labeled as JavaScript with a no-sniff header",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",      // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"{ \"key\"", ": true }"},              // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        1,                                      // verdict_packet
    },
    {
        "Allowed: Empty response with PNG mime type",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "image/png",                       // response_content_type
        MimeType::kOthers,                 // canonical_mime_type
        MimeTypeBucket::kPublic,           // mime_type_bucket
        {},                                // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,              // protection_decision
        Verdict::kAllowBecauseOutOfData,        // verdict
        kVerdictPacketForInconclusiveSniffing,  // verdict_packet
    },
    {
        "Allowed: Empty response with PNG mime type and nosniff header",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "image/png",                        // response_content_type
        MimeType::kOthers,                  // canonical_mime_type
        MimeTypeBucket::kPublic,            // mime_type_bucket
        {},                                 // packets
        false,                              // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,              // protection_decision
        Verdict::kAllowBecauseOutOfData,        // verdict
        kVerdictPacketForInconclusiveSniffing,  // verdict_packet
    },

    // Allowed responses due to sniffing:
    {
        "Allowed: Cross-site script to JSONP labeled as HTML",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"foo({\"x\" : 3})"},              // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site script to JavaScript labeled as text",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"var x = 3;"},                    // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: JSON-like JavaScript labeled as text",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"{", "    \n", "var x = 3;\n", "console.log('hello');"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        2,                          // verdict_packet
    },

    {
        "Allowed: JSONP labeled as JSON",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/json",                       // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"invoke({ \"key\": true });"},    // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed (for now): JSON array literal labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",      // target_url
        "http://www.a.com/",                   // initiator_origin
        "HTTP/1.1 200 OK",                     // response_headers
        "text/plain",                          // response_content_type
        MimeType::kPlain,                      // canonical_mime_type
        MimeTypeBucket::kProtected,            // mime_type_bucket
        {"[1, 2, {}, true, false, \"yay\"]"},  // packets
        false,                                 // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: JSON array literal on which a function is called.",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"[1, 2, {}, true, false, \"yay\"]", ".map(x => console.log(x))",
         ".map(x => console.log(x));"},  // packets
        false,                           // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to nonsense labeled as XML",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "application/xml",                 // response_content_type
        MimeType::kXml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"Won't sniff as XML"},            // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to nonsense labeled as JSON",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/json",                       // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"Won't sniff as JSON"},           // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: Cross-site XHR to partial match for <HTML> tag",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<htm"},                          // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,              // protection_decision
        Verdict::kAllowBecauseOutOfData,        // verdict
        kVerdictPacketForInconclusiveSniffing,  // verdict_packet
    },
    {
        "Allowed: HTML tag appears only after net::kMaxBytesToSniff",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {kHTMLWithTooLongComment},         // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,              // protection_decision
        Verdict::kAllowBecauseOutOfData,        // verdict
        kVerdictPacketForInconclusiveSniffing,  // verdict_packet
    },
    {
        "Allowed: Empty response with html mime type",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {},                                // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,              // protection_decision
        Verdict::kAllowBecauseOutOfData,        // verdict
        kVerdictPacketForInconclusiveSniffing,  // verdict_packet
    },
    {
        "Allowed: Same-origin XHR to a filesystem URI",
        __LINE__,
        "filesystem:http://www.a.com/file.html",    // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Allowed: Same-origin XHR to a blob URI",
        __LINE__,
        "blob:http://www.a.com/guid-goes-here",     // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },

    // Blocked responses (without sniffing):
    {
        "Blocked: Cross-site XHR to nosniff HTML without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Blocked: nosniff + Content-Type: text/html; charset=utf-8",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html; charset=utf-8",                 // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to nosniff response without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",      // response_headers
        "text/html",                            // response_content_type
        MimeType::kHtml,                        // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {"Wouldn't sniff as HTML"},             // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Blocked: Cross-origin, same-site XHR to nosniff HTML without CORS",
        __LINE__,
        "https://foo.site.com/resource.html",  // target_url
        "https://bar.site.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker/html/nosniff",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://c.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "text/html",                        // response_content_type
        MimeType::kHtml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,         // mime_type_bucket
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },

    {
        // This scenario is unusual, since there's no difference between
        // a blocked response and a non-blocked response.
        "Blocked(-ish?): Nosniff header + empty response",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",      // response_headers
        "text/html",                            // response_content_type
        MimeType::kHtml,                        // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {},                                     // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,  // protection_decision
        Verdict::kBlock,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },

    // CORB only applies to `no-cors` responses.
    {
        "Allowed: CORB N/A for CORS requests: Cross-site XHR + same-site CORS",
        // Note that initiator_origin is cross-origin, but same-site in relation
        // to the CORS response (the Access-Control-Allow-Origin header).
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://foo.example.com/",         // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://example.com",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<hTmL><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: CORB N/A for CORS requests: Cross-site XHR with wrong CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://example.com",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<hTmL><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Allowed: CORB N/A for CORS requests: JSON parser-breaker + wrong CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://example.com\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "application/javascript",           // response_content_type
        MimeType::kOthers,                  // canonical_mime_type
        MimeTypeBucket::kPublic,            // mime_type_bucket
        {")]}'\n[true, false]"},            // packets
        true,                               // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },

    // Blocked responses due to sniffing:
    {
        "Blocked: Cross-site XHR to HTML without CORS",
        __LINE__,
        "http://www.b.com/resource.html",           // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to XML without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "application/xml",                 // response_content_type
        MimeType::kXml,                    // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to JSON without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "application/json",                // response_content_type
        MimeType::kJson,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"{\"x\" : 3}"},                   // packets
        false,                             // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: slow-arriving JSON labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",             // target_url
        "http://www.a.com/",                          // initiator_origin
        "HTTP/1.1 200 OK",                            // response_headers
        "text/plain",                                 // response_content_type
        MimeType::kPlain,                             // canonical_mime_type
        MimeTypeBucket::kProtected,                   // mime_type_bucket
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        false,                                        // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        5,                          // verdict_packet
    },
    {
        "Blocked: slow-arriving xml labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",              // target_url
        "http://www.a.com/",                           // initiator_origin
        "HTTP/1.1 200 OK",                             // response_headers
        "text/plain",                                  // response_content_type
        MimeType::kPlain,                              // canonical_mime_type
        MimeTypeBucket::kProtected,                    // mime_type_bucket
        {"    ", "\t", "<", "?", "x", "m", "l", ">"},  // packets
        false,                                         // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        6,                          // verdict_packet
    },
    {
        "Blocked: slow-arriving html labeled as text/plain",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"    <!--", "\t -", "-", "->", "\n", "<", "s", "c", "r", "i", "p",
         "t"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        11,                         // verdict_packet
    },
    {
        "Blocked: slow-arriving html with commented-out xml tag",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/plain",                      // response_content_type
        MimeType::kPlain,                  // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"    <!--", " <?xml ", "-->\n", "<", "h", "e", "a", "d"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        7,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to HTML labeled as text without CORS",
        __LINE__,
        "http://www.b.com/resource.html",           // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/plain",                               // response_content_type
        MimeType::kPlain,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site <script> inclusion of HTML w/ DTD without CORS",
        __LINE__,
        "http://www.b.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK",                 // response_headers
        "text/html",                       // response_content_type
        MimeType::kHtml,                   // canonical_mime_type
        MimeTypeBucket::kProtected,        // mime_type_bucket
        {"<!doc", "type html><html itemscope=\"\" ",
         "itemtype=\"http://schema.org/SearchResultsPage\" ",
         "lang=\"en\"><head>"},  // packets
        false,                   // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        "Blocked: Cross-site fetch HTML from NaCl without CORS response",
        __LINE__,
        "http://www.b.com/plugin.html",             // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // first_chunk
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker and JSON mime type",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://c.com/",               // initiator_origin
        "HTTP/1.1 200 OK",             // response_headers
        "text/json",                   // response_content_type
        MimeType::kJson,               // canonical_mime_type
        MimeTypeBucket::kProtected,    // mime_type_bucket
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker/nosniff/other mime type",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://c.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff",  // response_headers
        "application/octet-stream",         // response_content_type
        MimeType::kOthers,                  // canonical_mime_type
        MimeTypeBucket::kPublic,            // mime_type_bucket
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        "Blocked: Cross-site JSON with parser breaker and other mime type",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://c.com/",               // initiator_origin
        "HTTP/1.1 200 OK",             // response_headers
        "application/javascript",      // response_content_type
        MimeType::kOthers,             // canonical_mime_type
        MimeTypeBucket::kPublic,       // mime_type_bucket
        {"for(;;)", ";[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        1,                          // verdict_packet
    },
    {
        // Test based on wpt/.../corb/.../css-with-json-parser-breaker.css
        "Blocked: Cross-site CSS with parser breaker and text/css mime type",
        __LINE__,
        "http://a.com/resource.css",            // target_url
        "http://c.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                      // response_headers
        "text/css",                             // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {R"()]}'
            {}
            #header { color: red; } )"},        // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        // Test based on http/tests/security/resources/xorigincss1.css
        "Blocked: Cross-site HTML/CSS polyglot with text/css mime type",
        __LINE__,
        "http://a.com/resource.css",            // target_url
        "http://c.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                      // response_headers
        "text/css",                             // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {R"(  <html>{}\n"
              .id3 {
                background-color: yellow;
              }
              </html> )"},                      // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to a filesystem URI",
        __LINE__,
        "filesystem:http://www.b.com/file.html",    // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    {
        "Blocked: Cross-site XHR to a blob URI",
        __LINE__,
        "blob:http://www.b.com/guid-goes-here",     // target_url
        "http://www.a.com/",                        // initiator_origin
        "HTTP/1.1 200 OK",                          // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },
    // Range response. The product code doesn't currently look at the exact
    // range specified, so we can get away with testing with arbitrary/random
    // values.
    {
        "Blocked-by-ORB: Javascript 206",
        __LINE__,
        "http://www.b.com/script.js",  // target_url
        "http://www.a.com/",           // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"x = 1;"},                             // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        // Here the resources is allowed cross-origin from b.com to a.com
        // because of the CORS header. However CORB still blocks c.com from
        // accessing it (so the protection decision should be kBlock).
        "Allowed: text/html 206 media with CORS",
        __LINE__,
        "http://www.b.com/movie.html",  // target_url
        "http://www.a.com/",            // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                             // response_content_type
        MimeType::kHtml,                         // canonical_mime_type
        MimeTypeBucket::kProtected,              // mime_type_bucket
        {"simulated *middle*-of-html content"},  // packets
        true,                                    // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,   // protection_decision
        Verdict::kAllow,                         // verdict
        kVerdictPacketForHeadersBasedVerdict,    // verdict_packet
    },
    {
        "Blocked-by-ORB: text/plain 206 media",
        __LINE__,
        "http://www.b.com/movie.txt",  // target_url
        "http://www.a.com/",           // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "text/plain",                           // response_content_type
        MimeType::kPlain,                       // canonical_mime_type
        MimeTypeBucket::kProtected,             // mime_type_bucket
        {"movie content"},                      // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Blocked: text/html 206 media",
        __LINE__,
        "http://www.b.com/movie.html",  // target_url
        "http://www.a.com/",            // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",   // response_headers
        "text/html",                             // response_content_type
        MimeType::kHtml,                         // canonical_mime_type
        MimeTypeBucket::kProtected,              // mime_type_bucket
        {"these middle bytes are unsniffable"},  // packets
        false,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,   // protection_decision
        Verdict::kBlock,                         // verdict
        kVerdictPacketForHeadersBasedVerdict,    // verdict_packet
    },
    {
        "Blocked-by-ORB: application/octet-stream 206 (middle)",
        __LINE__,
        "http://www.b.com/movie.html",  // target_url
        "http://www.a.com/",            // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",   // response_headers
        "application/octet-stream",              // response_content_type
        MimeType::kOthers,                       // canonical_mime_type
        MimeTypeBucket::kProtected,              // mime_type_bucket
        {"these middle bytes are unsniffable"},  // packets
        false,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,   // protection_decision
        Verdict::kAllow,                         // verdict
        kVerdictPacketForHeadersBasedVerdict,    // verdict_packet
    },
    {
        "Allowed: application/octet-stream 206 media - beginning of video",
        __LINE__,
        "http://www.b.com/movie.mp4",  // target_url
        "http://www.a.com/",           // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 0-800/67589",  // response_headers
        "application/octet-stream",          // response_content_type
        MimeType::kOthers,                   // canonical_mime_type
        MimeTypeBucket::kProtected,          // mime_type_bucket
        // Body of test response is based on:
        // 1) net/base/mime_sniffer.cc
        // 2) https://mimesniff.spec.whatwg.org/#signature-for-mp4
        {"....ftypmp4...."},                    // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Allowed: video/mp4 206 media - beginning of resource",
        __LINE__,
        "http://www.b.com/movie.mp4",  // target_url
        "http://www.a.com/",           // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 0-800/67589",  // response_headers
        "video/mp4",                         // response_content_type
        MimeType::kOthers,                   // canonical_mime_type
        MimeTypeBucket::kProtected,          // mime_type_bucket
        // Body of test response is based on:
        // 1) net/base/mime_sniffer.cc
        // 2) https://mimesniff.spec.whatwg.org/#signature-for-mp4
        {"MIME type means this doesn't have to sniff as video"},  // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Allowed: video/mp4 206 media - middle of resource",
        __LINE__,
        "http://www.b.com/movie.mp4",  // target_url
        "http://www.a.com/",           // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Content-Range: bytes 200-1000/67589",   // response_headers
        "video/mp4",                             // response_content_type
        MimeType::kOthers,                       // canonical_mime_type
        MimeTypeBucket::kProtected,              // mime_type_bucket
        {"these middle bytes are unsniffable"},  // packets
        false,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,   // protection_decision
        Verdict::kAllow,                         // verdict
        kVerdictPacketForHeadersBasedVerdict,    // verdict_packet
    },
    // Responses with no data.
    {
        "Allowed: same-origin 204 response with no data",
        __LINE__,
        "http://a.com/resource.html",              // target_url
        "http://a.com/",                           // initiator_origin
        "HTTP/1.1 204 NO CONTENT",                 // response_headers
        "text/html",                               // response_content_type
        MimeType::kHtml,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                // mime_type_bucket
        {/* empty body doesn't sniff as html */},  // packets
        false,                                     // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    {
        "Allowed after sniffing: cross-site 204 response with no data",
        __LINE__,
        "http://a.com/resource.html",              // target_url
        "http://b.com/",                           // initiator_origin
        "HTTP/1.1 204 NO CONTENT",                 // response_headers
        "text/html",                               // response_content_type
        MimeType::kHtml,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                // mime_type_bucket
        {/* empty body doesn't sniff as html */},  // packets
        false,                                     // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,              // protection_decision
        Verdict::kAllowBecauseOutOfData,        // verdict
        kVerdictPacketForInconclusiveSniffing,  // verdict_packet
    },

    // Testing the CORB protection logging.
    {
        "Not Sensitive: script without CORS or Cache heuristic",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin",                         // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Not Sensitive: vary user-agent is present and should be ignored",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin, User-Agent",             // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Not Sensitive: cache-control no-store should be ignored",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: No-Store",              // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    // Responses with the Access-Control-Allow-Origin header value other than *.
    {
        "Sensitive, Allowed: script with CORS heuristic and range header",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Vary: Origin\n"
        "Access-Control-Allow-Origin: http://www.a.com/\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Sensitive, Blocked: html with CORS heuristic and no sniff",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Sensitive, Blocked after sniffing: html with CORS heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive, Allowed after sniffing: javascript with CORS heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "application/javascript",  // response_content_type
        MimeType::kOthers,         // canonical_mime_type
        MimeTypeBucket::kPublic,   // mime_type_bucket
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive slow-arriving JSON with CORS heurisitic. Only needs "
        "sniffing for the CORP protection statistics.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/json",                                  // response_content_type
        MimeType::kJson,                              // canonical_mime_type
        MimeTypeBucket::kProtected,                   // mime_type_bucket
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        true,                                         // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        5,                          // verdict_packet
    },

    // Responses with Vary: Origin and Cache-Control: Private headers.
    {
        "Sensitive, Allowed: script with cache heuristic and range header",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Sensitive, Allowed: script with cache heuristic and range "
        "header. Has vary user agent + cache no store which should not "
        "confuse the cache heuristic.",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Vary: Origin, User-Agent\n"
        "Cache-Control: Private, No-Store\n"
        "Content-Range: bytes 200-1000/67589",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    {
        "Sensitive, Blocked: html with cache heuristic and no sniff",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Sensitive, Blocked after sniffing: html with cache heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive, Allowed after sniffing: javascript with cache heuristic",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",  // response_headers
        "application/javascript",  // response_content_type
        MimeType::kOthers,         // canonical_mime_type
        MimeTypeBucket::kPublic,   // mime_type_bucket
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive slow-arriving JSON with cache heurisitic. Only needs "
        "sniffing for the CORP protection statistics.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                     // response_headers
        "text/json",                                  // response_content_type
        MimeType::kJson,                              // canonical_mime_type
        MimeTypeBucket::kProtected,                   // mime_type_bucket
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        true,                                         // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        5,                          // verdict_packet
    },

    // The next two tests together ensure that when CORB blocks and strips the
    // cache/vary headers from a sensitive response the CORB protection logging
    // still correctly identifies the response as sensitive and reports it.
    //
    // In this first test, the protection logging reports immediately (without
    // sniffing). So we can (somewhat) safely assume the response is being
    // correctly reported as sensitive. Thus this test ensures the testing
    // infrastructure itself is also correctly idenitfying the response as
    // sensitive.
    {
        "Sensitive cache heuristic, both CORB and the protection stats block",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.b.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kBlock,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    // Here the protection logging only reports after sniffing. Despite this,
    // the resource should still be identified as sensitive.
    {
        "Sensitive cache heuristic, both CORB and the protection stats block",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.b.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },

    // A test that makes sure we don't double log the CORB protection stats.
    // Because the request is cross-origin + same-site and has a same-site CORP
    // header, CORB needs to sniff. However CORB protection logging makes the
    // request cross-site and so needs no sniffing. We don't want the protection
    // logging to be triggered a second time after the sniffing.
    {
        "Sensitive, CORB and CORB protection stats need sniffing",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.foo.a.com/",           // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Cross-Origin-Resource-Policy: same-site\n"
        "Vary: Origin\n"
        "Cache-Control: Private",                   // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kBlock,            // verdict
        0,                          // verdict_packet
    },

    // Response with an unknown MIME type.
    {
        "Sensitive, Allowed: unknown MIME type with CORS heuristic and range "
        "header",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 206 OK\n"
        "Vary: Origin\n"
        "Content-Range: bytes 200-1000/67589\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "unknown/mime_type",                    // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kOther,                 // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },

    // Responses with the accept-ranges header.
    {
        "Sensitive response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: bytes\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Sensitive response with an accept-ranges header but value |none|.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: none\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Non-sensitive response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: bytes",                     // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,             // protection_decision
        Verdict::kAllow,                       // verdict
        kVerdictPacketForHeadersBasedVerdict,  // verdict_packet
    },
    // Sensitive responses with the accept-ranges header, a protected MIME type
    // and protection decision = kBlock.
    {
        "CORS-heuristic response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: bytes\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Cache-heuristic response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: bytes\n"
        "Cache-Control: private\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Cache + CORS heuristics, accept-ranges header says |none|.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: none\n"
        "Cache-Control: private\n"
        "Access-Control-Allow-Origin: http://www.a.com/\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    // Sensitive responses with the accept-ranges header, a protected MIME type
    // and protection decision = kBlockedAfterSniffing. (These tests are
    // identical to the previous 3, except they lack a nosniff header.)
    {
        "CORS-heuristic response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: bytes\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Cache-heuristic response with an accept-ranges header.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: bytes\n"
        "Cache-Control: private\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Cache + CORS heuristics, accept-ranges header says |none|.",
        __LINE__,
        "http://www.a.com/resource.html",  // target_url
        "http://www.a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Accept-Ranges: none\n"
        "Cache-Control: private\n"
        "Access-Control-Allow-Origin: http://www.a.com/\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    // A Sensitive response with the accept-ranges header and a protected MIME
    // type but protection decision = kAllow (so the secondary accept-ranges
    // stats should not be reported).
    {
        "Sensitive + accept-ranges header but protection decision = kAllow.",
        __LINE__,
        "http://www.a.com/resource.js",  // target_url
        "http://www.a.com/",             // initiator_origin
        "HTTP/1.1 206 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Accept-Ranges: bytes\n"
        "Content-Range: bytes 200-1000/67589\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "application/javascript",               // response_content_type
        MimeType::kOthers,                      // canonical_mime_type
        MimeTypeBucket::kPublic,                // mime_type_bucket
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
        CrossOriginProtectionDecision::kAllow,  // protection_decision
        Verdict::kAllow,                        // verdict
        kVerdictPacketForHeadersBasedVerdict,   // verdict_packet
    },
    // Sensitive responses with no data.
    {
        "Sensitive, allowed after sniffing: same-origin 204 with no data",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://a.com/",               // initiator_origin
        "HTTP/1.1 204 NO CONTENT\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                               // response_content_type
        MimeType::kHtml,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                // mime_type_bucket
        {/* empty body doesn't sniff as html */},  // packets
        true,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,              // protection_decision
        Verdict::kAllowBecauseOutOfData,        // verdict
        kVerdictPacketForInconclusiveSniffing,  // verdict_packet
    },

    // These responses confirm we are correctly reporting when a nosniff header
    // is present. This should *only* be reported when a blocked, sensitive,
    // protected mime-type response has a nosniff header.
    //
    // This first response satisfies all criteria except it does not have a
    // nosniff header.
    {
        "Cache heuristic with no nosniff header",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "Cache-Control: private\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::
            kBlockedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    // These next responses have nosniff headers but are missing one of the
    // other criteria.
    {
        "Cache heuristic with nosniff header but not a protected type",
        __LINE__,
        "http://a.com/resource.js",  // target_url
        "http://a.com/",             // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Cache-Control: private\n"
        "Vary: origin",            // response_headers
        "application/javascript",  // response_content_type
        MimeType::kOthers,         // canonical_mime_type
        MimeTypeBucket::kPublic,   // mime_type_bucket
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
        CrossOriginProtectionDecision::
            kAllowedAfterSniffing,  // protection_decision
        Verdict::kAllow,            // verdict
        0,                          // verdict_packet
    },
    {
        "Cache heuristic with nosniff header and protection decision == kBlock",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: *\n"
        "Cache-Control: private\n"
        "Vary: origin",                             // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    // These responses satisfies all criteria and should report its nosniff
    // header.
    {
        "Nosniff header and satisfies the CORS heuristic",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Access-Control-Allow-Origin: http://www.a.com/",  // response_headers
        "text/html",                                // response_content_type
        MimeType::kHtml,                            // canonical_mime_type
        MimeTypeBucket::kProtected,                 // mime_type_bucket
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,      // protection_decision
        Verdict::kAllow,                            // verdict
        kVerdictPacketForHeadersBasedVerdict,       // verdict_packet
    },
    {
        "Nosniff header and satisfies the Cache heuristic",
        __LINE__,
        "http://a.com/resource.html",  // target_url
        "http://a.com/",               // initiator_origin
        "HTTP/1.1 200 OK\n"
        "X-Content-Type-Options: nosniff\n"
        "Cache-Control: private\n"
        "Vary: origin",                            // response_headers
        "text/html",                               // response_content_type
        MimeType::kHtml,                           // canonical_mime_type
        MimeTypeBucket::kProtected,                // mime_type_bucket
        {/* empty body doesn't sniff as html */},  // packets
        true,                                      // resource_is_sensitive
        CrossOriginProtectionDecision::kBlock,     // protection_decision
        Verdict::kAllow,                           // verdict
        kVerdictPacketForHeadersBasedVerdict,      // verdict_packet
    },
};

}  // namespace

// Tests that verify ResponseAnalyzer correctly classifies network responses as
// allowed or blocked.
//
// The various test cases are passed as a list of TestScenario structs.
class ResponseAnalyzerTest : public testing::Test,
                             public testing::WithParamInterface<TestScenario> {
 public:
  ResponseAnalyzerTest()
      : context_(net::CreateTestURLRequestContextBuilder()->Build()) {}

  ResponseAnalyzerTest(const ResponseAnalyzerTest&) = delete;
  ResponseAnalyzerTest& operator=(const ResponseAnalyzerTest&) = delete;

  // Returns a ResourceResponse that matches the TestScenario's parameters.
  mojom::URLResponseHeadPtr CreateResponse(
      const std::string& response_content_type,
      const std::string& raw_response_headers,
      const std::string& initiator_origin) {
    auto response = mojom::URLResponseHead::New();
    std::string formatted_response_headers =
        net::HttpUtil::AssembleRawHeaders(raw_response_headers);
    scoped_refptr<net::HttpResponseHeaders> response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            formatted_response_headers);

    std::string charset;
    bool had_charset = false;
    response_headers->SetHeader("Content-Type", response_content_type);
    net::HttpUtil::ParseContentType(response_content_type, &response->mime_type,
                                    &charset, &had_charset, nullptr);
    EXPECT_FALSE(response->mime_type.empty())
        << "Invalid MIME type defined in kScenarios.";
    response->headers = response_headers;

    return response;
  }

  // Take and run ResponseAnalyzer on the current scenario. Allow the analyzer
  // to sniff the response body if needed and confirm it correctly decides to
  // block or allow.
  void RunAnalyzerOnScenario(const TestScenario& scenario,
                             const mojom::URLResponseHead& response,
                             std::unique_ptr<ResponseAnalyzer> analyzer,
                             bool verify_when_decision_is_made = true) {
    // Initialize |request| from the parameters.
    std::unique_ptr<net::URLRequest> request = context_->CreateRequest(
        GURL(scenario.target_url), net::DEFAULT_PRIORITY, &delegate_,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_initiator(
        url::Origin::Create(GURL(scenario.initiator_origin)));

    // Check if this is a CORS request.
    std::string cors_header_value;
    response.headers->GetNormalizedHeader("access-control-allow-origin",
                                          &cors_header_value);
    auto request_mode = cors_header_value == "" ? mojom::RequestMode::kNoCors
                                                : mojom::RequestMode::kCors;

    // Initialize the `analyzer`.
    //
    // Note that the `analyzer` will be destructed when `analyzer` goes out of
    // scope (the destructor may trigger logging of UMAs that some callers of
    // RunAnalyzerOnScenario attempt to verify).
    ResponseAnalyzer::Decision decision =
        analyzer->Init(request->url(), request->initiator(), request_mode,
                       mojom::RequestDestination::kEmpty, response);

    // This vector holds the packets to be delivered.
    std::vector<const char*> packets_vector(scenario.packets);
    packets_vector.push_back(
        "");  // End-of-stream is marked by an empty packet.

    // If the |verdict_packet| == kVerdictPacketForHeadersBasedVerdict = -1,
    // then the sniffing loop below will be skipped.
    if (scenario.verdict_packet != kVerdictPacketForInconclusiveSniffing &&
        scenario.verdict_packet != kVerdictPacketForHeadersBasedVerdict) {
      EXPECT_LT(scenario.verdict_packet,
                static_cast<int>(packets_vector.size()));
    }

    // Verify that the ResponseAnalyzer asks for sniffing if this is what the
    // testcase expects.
    if (verify_when_decision_is_made) {
      bool expected_to_sniff =
          scenario.verdict_packet != kVerdictPacketForHeadersBasedVerdict;
      if (expected_to_sniff) {
        EXPECT_EQ(decision, ResponseAnalyzer::Decision::kSniffMore);
      } else {
        // If we don't expect to sniff then ResponseAnalyzer should have already
        // made a blockng decision based on the headers.
        if (scenario.verdict == Verdict::kBlock) {
          EXPECT_EQ(decision, ResponseAnalyzer::Decision::kBlock);
        } else {
          EXPECT_EQ(decision, ResponseAnalyzer::Decision::kAllow);
        }
      }
    }

    // Simulate the behaviour of the URLLoader by appending the packets into
    // |data_buffer| and feeding this to |analyzer|.
    bool run_out_of_data_to_sniff = false;
    int actual_verdict_packet = kVerdictPacketForHeadersBasedVerdict;
    if (decision == ResponseAnalyzer::Decision::kSniffMore) {
      std::string data_buffer;
      size_t data_offset = 0;
      for (size_t packet_index = 0; packet_index < packets_vector.size();
           packet_index++) {
        SCOPED_TRACE(testing::Message()
                     << "While delivering packet #" << packet_index);

        // At each iteration of the loop we feed a new packet to |analyzer|,
        // expecting to get a decision at the |verdict_packet|. Since we haven't
        // given the next packet to |analyzer| yet at this point in the loop, it
        // shouldn't have made a decision yet.
        EXPECT_EQ(decision, ResponseAnalyzer::Decision::kSniffMore);

        // Append the next packet of the response body. If appending the entire
        // packet would exceed net::kMaxBytesToSniff we truncate the data.
        size_t bytes_to_append = strlen(packets_vector[packet_index]);
        if (data_offset + bytes_to_append > net::kMaxBytesToSniff)
          bytes_to_append = net::kMaxBytesToSniff - data_offset;
        data_buffer.append(packets_vector[packet_index], bytes_to_append);

        // Hand |analyzer_| the data to sniff.
        decision = analyzer->Sniff(data_buffer);
        data_offset += bytes_to_append;
        if (decision != ResponseAnalyzer::Decision::kSniffMore) {
          actual_verdict_packet = packet_index;
          break;
        }
      }
    }

    // Handle scenarios where no decision can be made before running out of data
    // to sniff.
    if (decision == ResponseAnalyzer::Decision::kSniffMore) {
      actual_verdict_packet = kVerdictPacketForInconclusiveSniffing;
      run_out_of_data_to_sniff = true;
      decision = analyzer->HandleEndOfSniffableResponseBody();

      // HandleEndOfSniffableResponseBody should never return kSniffMore.
      EXPECT_NE(decision, ResponseAnalyzer::Decision::kSniffMore);
    }

    // Confirm the analyzer is blocking or allowing correctly (now that we have
    // performed any needed sniffing).
    if (verify_when_decision_is_made) {
      EXPECT_EQ(scenario.verdict_packet, actual_verdict_packet);
    }
    if (scenario.verdict == Verdict::kBlock) {
      EXPECT_EQ(decision, ResponseAnalyzer::Decision::kBlock);
    } else {
      EXPECT_EQ(decision, ResponseAnalyzer::Decision::kAllow);

      if (verify_when_decision_is_made) {
        // In this case either the |analyzer| has decided to allow the response,
        // or run out of data and so the response will be allowed by default.
        if (scenario.verdict == Verdict::kAllow) {
          EXPECT_FALSE(run_out_of_data_to_sniff);
        } else {
          EXPECT_EQ(Verdict::kAllowBecauseOutOfData, scenario.verdict);
          EXPECT_TRUE(run_out_of_data_to_sniff);
        }
      }
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  net::TestDelegate delegate_;
};  // namespace network

// Runs a particular TestScenario (passed as the test's parameter) through the
// ResponseAnalyzer, verifying that the response is correctly allowed or blocked
// based on the scenario.
TEST_P(ResponseAnalyzerTest, ResponseBlocking) {
  const TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);
  base::HistogramTester histograms;
  // Initialize |response| from the parameters.
  //
  // TODO(krstnmnlsn): The response is constructed outside of
  // RunAnalyzerOnScenario to let the CORBProtection test below gather some
  // values from it before the analyzer clears the headers. Once the
  // CORBProtection logging is removed (and that test gone) the response can be
  // constructed in RunAnalyzerOnScenario().
  auto response =
      CreateResponse(scenario.response_content_type, scenario.response_headers,
                     scenario.initiator_origin);

  // Run the analyzer and confirm it allows/blocks correctly.
  RunAnalyzerOnScenario(scenario, *response,
                        std::make_unique<CorbResponseAnalyzer>());

  // Verify that histograms are correctly incremented.
  base::HistogramTester::CountsMap expected_counts;
  std::string histogram_base = "SiteIsolation.XSD.Browser";
  switch (scenario.canonical_mime_type) {
    case MimeType::kHtml:
    case MimeType::kXml:
    case MimeType::kJson:
    case MimeType::kPlain:
    case MimeType::kOthers:
      break;
    case MimeType::kNeverSniffed:
      DCHECK_EQ(Verdict::kBlock, scenario.verdict);
      DCHECK_EQ(kVerdictPacketForHeadersBasedVerdict, scenario.verdict_packet);
      break;
    case MimeType::kInvalidMimeType:
      DCHECK_EQ(Verdict::kAllow, scenario.verdict);
      DCHECK_EQ(kVerdictPacketForHeadersBasedVerdict, scenario.verdict_packet);
      break;
  }
  // We don't expect to see a start action because that is triggered in the
  // URLLoader.
  bool should_be_blocked = scenario.verdict == Verdict::kBlock;
  bool expected_to_sniff =
      scenario.verdict_packet != kVerdictPacketForHeadersBasedVerdict;
  int end_action = -1;
  if (should_be_blocked && expected_to_sniff) {
    end_action = static_cast<int>(
        CrossOriginReadBlocking::Action::kBlockedAfterSniffing);
  } else if (should_be_blocked && !expected_to_sniff) {
    end_action = static_cast<int>(
        CrossOriginReadBlocking::Action::kBlockedWithoutSniffing);
  } else if (!should_be_blocked && expected_to_sniff) {
    end_action = static_cast<int>(
        CrossOriginReadBlocking::Action::kAllowedAfterSniffing);
  } else if (!should_be_blocked && !expected_to_sniff) {
    end_action = static_cast<int>(
        CrossOriginReadBlocking::Action::kAllowedWithoutSniffing);
  } else {
    NOTREACHED();
  }

  // Expecting two actions:
  // 1. kResponseStarted
  // 2. The action recording CORB's actual decision (`end_action`).
  expected_counts[histogram_base + ".Action"] = 2;
  const auto kResponseStarted = static_cast<base::HistogramBase::Sample>(
      CrossOriginReadBlocking::Action::kResponseStarted);
  EXPECT_THAT(histograms.GetAllSamples(histogram_base + ".Action"),
              testing::ElementsAre(base::Bucket(kResponseStarted, 1),
                                   base::Bucket(end_action, 1)))
      << "Should have incremented the right actions.";

  if (should_be_blocked)
    expected_counts[histogram_base + ".Blocked.CanonicalMimeType"] = 1;

  // Make sure that the expected metrics, and only those metrics, were
  // incremented.
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser"),
              testing::ContainerEq(expected_counts));
}

// Runs a particular TestScenario (passed as the test's parameter) through the
// ResponseAnalyzer, verifying that the expected CORB Protection UMA Histogram
// values are reported.
TEST_P(ResponseAnalyzerTest, CORBProtectionLogging) {
  const TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);
  base::HistogramTester histograms;
  // Initialize |response| from the parameters and record if it looks sensitive
  // or supports range requests. These values are saved because the analyzer
  // will clear the response headers in the event it decides to block.
  auto response =
      CreateResponse(scenario.response_content_type, scenario.response_headers,
                     scenario.initiator_origin);
  const bool seems_sensitive_from_cors_heuristic = CrossOriginReadBlocking::
      CorbResponseAnalyzer::SeemsSensitiveFromCORSHeuristic(*response);
  const bool seems_sensitive_from_cache_heuristic = CrossOriginReadBlocking::
      CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(*response);
  const bool supports_range_requests =
      CorbResponseAnalyzer::SupportsRangeRequests(*response);
  const bool expect_nosniff = CorbResponseAnalyzer::HasNoSniff(*response);

  // Run the analyzer and confirm it allows/blocks correctly.
  RunAnalyzerOnScenario(scenario, *response,
                        std::make_unique<CorbResponseAnalyzer>());

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["SiteIsolation.CORBProtection.SensitiveResource"] = 1;
  if (scenario.resource_is_sensitive) {
    // Check that we reported correctly if the server supports range
    // requests.
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.CORBProtection.SensitiveWithRangeSupport"),
                testing::ElementsAre(base::Bucket(supports_range_requests, 1)));
    expected_counts["SiteIsolation.CORBProtection.SensitiveWithRangeSupport"] =
        1;

    std::string mime_type_bucket;
    switch (scenario.mime_type_bucket) {
      case MimeTypeBucket::kProtected:
        mime_type_bucket = ".ProtectedMimeType";
        break;
      case MimeTypeBucket::kPublic:
        mime_type_bucket = ".PublicMimeType";
        break;
      case MimeTypeBucket::kOther:
        mime_type_bucket = ".OtherMimeType";
        break;
    }
    std::string blocked_with_range_support;
    switch (scenario.protection_decision) {
      case CrossOriginProtectionDecision::kBlock:
        blocked_with_range_support = ".BlockedWithRangeSupport";
        break;
      case CrossOriginProtectionDecision::kBlockedAfterSniffing:
        blocked_with_range_support = ".BlockedAfterSniffingWithRangeSupport";
        break;
      default:
        blocked_with_range_support = "This value should not be used.";
    }
    bool expect_range_support_histograms =
        scenario.mime_type_bucket == MimeTypeBucket::kProtected &&
        (scenario.protection_decision ==
             CrossOriginProtectionDecision::kBlock ||
         scenario.protection_decision ==
             CrossOriginProtectionDecision::kBlockedAfterSniffing);
    // In this scenario the file seemed sensitive so we expect a report. Note
    // there may be two reports if the resource satisfied both the CORS
    // heuristic and the Cache heuristic.
    std::string cors_base = "SiteIsolation.CORBProtection.CORSHeuristic";
    std::string cache_base = "SiteIsolation.CORBProtection.CacheHeuristic";
    std::string cors_protected = cors_base + ".ProtectedMimeType";
    std::string cache_protected = cache_base + ".ProtectedMimeType";
    std::string blocked_nosniff = ".BlockedWithoutSniffing.HasNoSniff";
    if (seems_sensitive_from_cors_heuristic) {
      expected_counts[cors_base + mime_type_bucket] = 1;
      EXPECT_THAT(histograms.GetAllSamples(cors_base + mime_type_bucket),
                  testing::ElementsAre(base::Bucket(
                      static_cast<int>(scenario.protection_decision), 1)))
          << "CORB should have reported the right protection decision.";
      if (expect_range_support_histograms) {
        EXPECT_THAT(
            histograms.GetAllSamples(cors_protected +
                                     blocked_with_range_support),
            testing::ElementsAre(base::Bucket(supports_range_requests, 1)));
        expected_counts[cors_protected + blocked_with_range_support] = 1;
      }
      if (scenario.mime_type_bucket == MimeTypeBucket::kProtected &&
          scenario.protection_decision ==
              CrossOriginProtectionDecision::kBlock) {
        EXPECT_THAT(histograms.GetAllSamples(cors_protected + blocked_nosniff),
                    testing::ElementsAre(base::Bucket(expect_nosniff, 1)));
        expected_counts[cors_protected + blocked_nosniff] = 1;
      }
    }
    if (seems_sensitive_from_cache_heuristic) {
      expected_counts[cache_base + mime_type_bucket] = 1;
      EXPECT_THAT(histograms.GetAllSamples(cache_base + mime_type_bucket),
                  testing::ElementsAre(base::Bucket(
                      static_cast<int>(scenario.protection_decision), 1)))
          << "CORB should have reported the right protection decision.";
      if (expect_range_support_histograms) {
        EXPECT_THAT(
            histograms.GetAllSamples(cache_protected +
                                     blocked_with_range_support),
            testing::ElementsAre(base::Bucket(supports_range_requests, 1)));
        expected_counts[cache_protected + blocked_with_range_support] = 1;
      }
      if (scenario.mime_type_bucket == MimeTypeBucket::kProtected &&
          scenario.protection_decision ==
              CrossOriginProtectionDecision::kBlock) {
        EXPECT_THAT(histograms.GetAllSamples(cache_protected + blocked_nosniff),
                    testing::ElementsAre(base::Bucket(expect_nosniff, 1)));
        expected_counts[cache_protected + blocked_nosniff] = 1;
      }
    }

    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("SiteIsolation.CORBProtection"),
        testing::ContainerEq(expected_counts));
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.CORBProtection.SensitiveResource"),
                testing::ElementsAre(base::Bucket(true, 1)));
  } else {
    // In this scenario the file should not have appeared sensitive, so only the
    // SensitiveResource boolean should have been reported (as false).
    EXPECT_THAT(
        histograms.GetTotalCountsForPrefix("SiteIsolation.CORBProtection"),
        testing::ContainerEq(expected_counts));
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.CORBProtection.SensitiveResource"),
                testing::ElementsAre(base::Bucket(false, 1)));
  }
}

TEST_P(ResponseAnalyzerTest, OpaqueResponseBlocking) {
  TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);

  // Unlike CORB, ORB blocks all 206 responses, unless there was an earlier
  // request to the same URL and that earlier request was classified (based on
  // the MIME type or sniffing) as an audio-or-video response.
  base::StringPiece description = scenario.description;
  if (description == "Blocked-by-ORB: text/plain 206 media" ||
      description == "Blocked-by-ORB: Javascript 206" ||
      description == "Blocked-by-ORB: application/octet-stream 206 (middle)") {
    scenario.verdict = Verdict::kBlock;
    scenario.verdict_packet = kVerdictPacketForHeadersBasedVerdict;
  }

  // Initialize |response| from the parameters and record if it looks sensitive
  // or supports range requests. These values are saved because the analyzer
  // will clear the response headers in the event it decides to block.
  auto response =
      CreateResponse(scenario.response_content_type, scenario.response_headers,
                     scenario.initiator_origin);

  // ORB may make the final decision at a different time than CORB.  This
  // makes no difference from functional/observable behavior perspective
  // and skipping this verification makes it easier to share testcases
  // across ORB and CORB.
  //
  // TODO(lukasza): Eventually `verify_when_decision_is_made` for ORB (once
  // CORB is removed at the latest, but possibly earlier than that).
  constexpr bool kVerifyWhenDecisionIsMade = false;

  PerFactoryState per_factory_state;
  auto analyzer =
      std::make_unique<OpaqueResponseBlockingAnalyzer>(per_factory_state);

  RunAnalyzerOnScenario(scenario, *response, std::move(analyzer),
                        kVerifyWhenDecisionIsMade);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ResponseAnalyzerTest,
                         ::testing::ValuesIn(kScenarios));

// =============================================================================
// The following individual tests check the behaviour of various methods in
// isolation.
// =============================================================================

TEST(CrossOriginReadBlockingTest, SniffForHTML) {
  using CORB = CrossOriginReadBlocking;

  // Something that technically matches the start of a valid HTML tag.
  EXPECT_EQ(SniffingResult::kYes,
            CORB::SniffForHTML("  \t\r\n    <HtMladfokadfkado"));

  // HTML comment followed by whitespace and valid HTML tags.
  EXPECT_EQ(SniffingResult::kYes,
            CORB::SniffForHTML(" <!-- this is comment -->\n<html><body>"));

  // HTML comment, whitespace, more HTML comments, HTML tags.
  EXPECT_EQ(
      SniffingResult::kYes,
      CORB::SniffForHTML(
          "<!-- this is comment -->\n<!-- this is comment -->\n<html><body>"));

  // HTML comment followed by valid HTML tag.
  EXPECT_EQ(
      SniffingResult::kYes,
      CORB::SniffForHTML("<!-- this is comment <!-- -->\n<script></script>"));

  // Whitespace followed by valid Javascript.
  EXPECT_EQ(SniffingResult::kNo,
            CORB::SniffForHTML("        var name=window.location;\nadfadf"));

  // HTML comment followed by valid Javascript.
  EXPECT_EQ(
      SniffingResult::kNo,
      CORB::SniffForHTML(
          " <!-- this is comment\n document.write(1);\n// -->\nwindow.open()"));

  // HTML/Javascript polyglot should return kNo.
  EXPECT_EQ(SniffingResult::kNo,
            CORB::SniffForHTML(
                "<!--/*--><html><body><script type='text/javascript'><!--//*/\n"
                "var blah = 123;\n"
                "//--></script></body></html>"));

  // Tests to cover more of MaybeSkipHtmlComment.
  EXPECT_EQ(SniffingResult::kMaybe, CORB::SniffForHTML("<!-- -/* --><html>"));
  EXPECT_EQ(SniffingResult::kMaybe, CORB::SniffForHTML("<!-- --/* --><html>"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- -/* -->\n<html>"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- --/* -->\n<html>"));
  EXPECT_EQ(SniffingResult::kMaybe, CORB::SniffForHTML("<!----> <html>"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!---->\n<html>"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!---->\r<html>"));
  EXPECT_EQ(SniffingResult::kYes,
            CORB::SniffForHTML("<!-- ---/-->\n<html><body>"));

  // HTML spec only allows *ASCII* whitespace before the first html element.
  // See also https://html.spec.whatwg.org/multipage/syntax.html and
  // https://infra.spec.whatwg.org/#ascii-whitespace.
  EXPECT_EQ(SniffingResult::kNo, CORB::SniffForHTML("<!---->\u2028<html>"));
  EXPECT_EQ(SniffingResult::kNo, CORB::SniffForHTML("<!---->\u2029<html>"));

  // Order of line terminators.
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- -->\n<b>\rx"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- -->\r<b>\nx"));
  EXPECT_EQ(SniffingResult::kNo, CORB::SniffForHTML("<!-- -->\nx\r<b>"));
  EXPECT_EQ(SniffingResult::kNo, CORB::SniffForHTML("<!-- -->\rx\n<b>"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- -->\n<b>\u2028x"));
  EXPECT_EQ(SniffingResult::kNo, CORB::SniffForHTML("<!-- -->\u2028<b>\n<b>"));

  // In UTF8 encoding <LS> is 0xE2 0x80 0xA8 and <PS> is 0xE2 0x80 0xA9.
  // Let's verify that presence of 0xE2 alone doesn't throw
  // FindFirstJavascriptLineTerminator into an infinite loop.
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- --> \xe2 \n<b"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- --> \xe2\x80 \n<b"));
  EXPECT_EQ(SniffingResult::kYes, CORB::SniffForHTML("<!-- --> \x80 \n<b"));

  // Commented out html tag followed by non-html (" x").
  StringPiece commented_out_html_tag_data("<!-- <html> <?xml> \n<html>-->\nx");
  EXPECT_EQ(SniffingResult::kNo,
            CORB::SniffForHTML(commented_out_html_tag_data));

  // Prefixes of |commented_out_html_tag_data| should be indeterminate.
  // This covers testing "<!-" as well as "<!-- not terminated yet...".
  StringPiece almost_html = commented_out_html_tag_data;
  while (!almost_html.empty()) {
    almost_html.remove_suffix(1);
    EXPECT_EQ(SniffingResult::kMaybe, CORB::SniffForHTML(almost_html))
        << almost_html;
  }

  // Explicit tests for an unfinished comment (some also covered by the prefix
  // tests above).
  EXPECT_EQ(SniffingResult::kMaybe, CORB::SniffForHTML(""));
  EXPECT_EQ(SniffingResult::kMaybe, CORB::SniffForHTML("<!"));
  EXPECT_EQ(SniffingResult::kMaybe, CORB::SniffForHTML("<!-- unterminated..."));
  EXPECT_EQ(SniffingResult::kMaybe,
            CORB::SniffForHTML("<!-- blah --> <html> no newline yet"));
}

TEST(CrossOriginReadBlockingTest, SniffForXML) {
  StringPiece xml_data("   \t \r \n     <?xml version=\"1.0\"?>\n <catalog");
  StringPiece non_xml_data("        var name=window.location;\nadfadf");
  StringPiece empty_data("");

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForXML(xml_data));
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForXML(non_xml_data));

  // Empty string should be indeterminate.
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForXML(empty_data));
}

TEST(CrossOriginReadBlockingTest, SniffForJSON) {
  StringPiece json_data("\t\t\r\n   { \"name\" : \"chrome\", ");
  StringPiece json_corrupt_after_first_key(
      "\t\t\r\n   { \"name\" :^^^^!!@#\1\", ");
  StringPiece json_data2("{ \"key   \\\"  \"          \t\t\r\n:");
  StringPiece non_json_data0("\t\t\r\n   { name : \"chrome\", ");
  StringPiece non_json_data1("\t\t\r\n   foo({ \"name\" : \"chrome\", ");

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(json_data));
  EXPECT_EQ(SniffingResult::kYes, CrossOriginReadBlocking::SniffForJSON(
                                      json_corrupt_after_first_key));

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(json_data2));

  // All prefixes prefixes of |json_data2| ought to be indeterminate.
  StringPiece almost_json = json_data2;
  while (!almost_json.empty()) {
    almost_json.remove_suffix(1);
    EXPECT_EQ(SniffingResult::kMaybe,
              CrossOriginReadBlocking::SniffForJSON(almost_json))
        << almost_json;
  }

  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(non_json_data0));
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(non_json_data1));

  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(R"({"" : 1})"))
      << "Empty strings are accepted";
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(R"({'' : 1})"))
      << "Single quotes are not accepted";
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON("{\"\\\"\" : 1}"))
      << "Escaped quotes are recognized";
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON(R"({"\\\u000a" : 1})"))
      << "Escaped control characters are recognized";
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForJSON(R"({"\\\u00)"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForJSON("{\"\\"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kMaybe,
            CrossOriginReadBlocking::SniffForJSON("{\"\\\""))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kYes,
            CrossOriginReadBlocking::SniffForJSON("{\"\n\" : true}"))
      << "Unescaped control characters are accepted (a bit more like "
      << "Javascript than strict reading of the JSON spec)";
  EXPECT_EQ(SniffingResult::kNo, CrossOriginReadBlocking::SniffForJSON("{}"))
      << "Empty dictionary is not recognized (since it's valid JS too)";
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON("[true, false, 1, 2]"))
      << "Lists dictionary are not recognized (since they're valid JS too)";
  EXPECT_EQ(SniffingResult::kNo,
            CrossOriginReadBlocking::SniffForJSON(R"({":"})"))
      << "A colon character inside a string does not trigger a match";
}

TEST(CrossOriginReadBlockingTest, GetCanonicalMimeType) {
  std::vector<std::pair<const char*, MimeType>> tests = {
      // Basic tests for things in the original implementation:
      {"text/html", MimeType::kHtml},
      {"text/xml", MimeType::kXml},
      {"application/rss+xml", MimeType::kXml},
      {"application/xml", MimeType::kXml},
      {"application/json", MimeType::kJson},
      {"text/json", MimeType::kJson},
      {"text/plain", MimeType::kPlain},

      // Other mime types:
      {"application/foobar", MimeType::kOthers},

      // Regression tests for https://crbug.com/799155 (prefix/suffix matching):
      {"application/activity+json", MimeType::kJson},
      {"text/foobar+xml", MimeType::kXml},
      // No match without a '+' character:
      {"application/jsonfoobar", MimeType::kOthers},
      {"application/foobarjson", MimeType::kOthers},
      {"application/xmlfoobar", MimeType::kOthers},
      {"application/foobarxml", MimeType::kOthers},

      // Case-insensitive comparison:
      {"APPLICATION/JSON", MimeType::kJson},
      {"APPLICATION/ACTIVITY+JSON", MimeType::kJson},
      {"appLICAtion/zIP", MimeType::kNeverSniffed},

      // Images are allowed cross-site, and SVG is an image, so we should
      // classify SVG as "other" instead of "xml" (even though it technically is
      // an xml document).  Same argument for DASH video format.
      {"image/svg+xml", MimeType::kOthers},
      {"application/dash+xml", MimeType::kOthers},

      // Javascript should not be blocked.
      {"application/javascript", MimeType::kOthers},
      {"application/jsonp", MimeType::kOthers},

      // TODO(lukasza): Remove in the future, once this MIME type is not used in
      // practice.  See also https://crbug.com/826756#c3
      {"application/json+protobuf", MimeType::kJson},
      {"APPLICATION/JSON+PROTOBUF", MimeType::kJson},

      // According to specs, these types are not XML or JSON.  See also:
      // - https://mimesniff.spec.whatwg.org/#xml-mime-type
      // - https://mimesniff.spec.whatwg.org/#json-mime-type
      {"text/x-json", MimeType::kOthers},
      {"text/json+blah", MimeType::kOthers},
      {"application/json+blah", MimeType::kOthers},
      {"text/xml+blah", MimeType::kOthers},
      {"application/xml+blah", MimeType::kOthers},

      // Types protected without sniffing.
      {"application/gzip", MimeType::kNeverSniffed},
      {"application/pdf", MimeType::kNeverSniffed},
      {"application/x-protobuf", MimeType::kNeverSniffed},
      {"application/x-gzip", MimeType::kNeverSniffed},
      {"application/zip", MimeType::kNeverSniffed},
      {"multipart/byteranges", MimeType::kNeverSniffed},
      {"multipart/signed", MimeType::kNeverSniffed},
      {"text/csv", MimeType::kNeverSniffed},
      {"text/event-stream", MimeType::kNeverSniffed},
  };

  for (const auto& test : tests) {
    const char* input = test.first;  // e.g. "text/html"
    MimeType expected = test.second;
    MimeType actual = CrossOriginReadBlocking::GetCanonicalMimeType(input);
    EXPECT_EQ(expected, actual)
        << "when testing with the following input: " << input;
  }
}

mojom::URLResponseHeadPtr CreateResponse(std::string raw_headers) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(raw_headers));

  auto response = mojom::URLResponseHead::New();
  response->headers = headers;
  return response;
}

TEST(CrossOriginReadBlockingTest, SeemsSensitiveFromCORSHeuristic) {
  // Response with no CORS header.
  auto no_cors_response = CreateResponse("HTTP/1.1 200 OK");
  EXPECT_FALSE(
      CorbResponseAnalyzer::SeemsSensitiveFromCORSHeuristic(*no_cors_response));

  // Response with CORS value = "*", so not sensitive.
  auto cors_any_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Access-Control-Allow-Origin: *");
  EXPECT_FALSE(CorbResponseAnalyzer::SeemsSensitiveFromCORSHeuristic(
      *cors_any_response));

  // Response with CORS value = "null", so not sensitive.
  auto cors_null_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Access-Control-Allow-Origin: null");
  EXPECT_FALSE(CorbResponseAnalyzer::SeemsSensitiveFromCORSHeuristic(
      *cors_null_response));

  // Response with CORS header restricting access to a particular origin.
  auto cors_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Access-Control-Allow-Origin: http://www.a.com/");
  EXPECT_TRUE(
      CorbResponseAnalyzer::SeemsSensitiveFromCORSHeuristic(*cors_response));
}

TEST(CrossOriginReadBlockingTest, SeemsSensitiveFromCacheHeuristic) {
  // Response with no cache-control or vary header.
  auto no_cache_response = CreateResponse("HTTP/1.1 200 OK");
  EXPECT_FALSE(CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(
      *no_cache_response));

  // Response with cache-control but no vary header.
  auto cache_only_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Cache-Control: private");
  EXPECT_FALSE(CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(
      *cache_only_response));

  // Response with vary: origin but no cache-control header.
  auto vary_only_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Vary: origin");
  EXPECT_FALSE(CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(
      *vary_only_response));

  // Response with vary: user-agent and cache-control: no-cache (should still
  // not seem sensitive).
  auto wrong_values_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Vary: user-agent\n"
      "Cache-Control: no-cache");
  EXPECT_FALSE(CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(
      *wrong_values_response));

  // Response with vary: origin and cache-control: private.
  auto vary_and_cache_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Vary: origin\n"
      "Cache-Control: private");
  EXPECT_TRUE(CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(
      *vary_and_cache_response));

  // Response with vary: origin, user-agent and cache-control: private, no-store
  // (we should still find the relevant values).
  auto extra_values_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Vary: origin, user-agent\n"
      "Cache-Control: no-cache, private");
  EXPECT_TRUE(CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(
      *extra_values_response));
}

TEST(CrossOriginReadBlockingTest, SeemsSensitiveWithBothHeuristics) {
  // Response with CORS heuristic should not appear sensitive to cache
  // heuristic.
  auto cors_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Access-Control-Allow-Origin: http://www.a.com/");
  EXPECT_FALSE(
      CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(*cors_response));

  // Response with cache heuristic should not appear sensitive to CORS
  // heuristic.
  auto cache_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Vary: origin\n"
      "Cache-Control: private");
  EXPECT_FALSE(
      CorbResponseAnalyzer::SeemsSensitiveFromCORSHeuristic(*cache_response));

  // Response with both cache and CORS heuristic signals (e.g. vary: origin +
  // cache-control: private as well as the access-control-allow-origin header).
  auto both_response = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Vary: origin\n"
      "Cache-Control: private\n"
      "Access-Control-Allow-Origin: http://www.a.com/");
  EXPECT_TRUE(
      CorbResponseAnalyzer::SeemsSensitiveFromCORSHeuristic(*both_response));
  EXPECT_TRUE(
      CorbResponseAnalyzer::SeemsSensitiveFromCacheHeuristic(*both_response));
}

TEST(CrossOriginReadBlockingTest, SupportsRangeRequests) {
  // Response with no Accept-Ranges header. Should return false.
  auto no_accept_ranges = CreateResponse("HTTP/1.1 200 OK");
  EXPECT_FALSE(CorbResponseAnalyzer::SupportsRangeRequests(*no_accept_ranges));

  // Response with an Accept-Ranges header. Should return true.
  auto bytes_accept_ranges = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Accept-Ranges: bytes");
  EXPECT_TRUE(
      CorbResponseAnalyzer::SupportsRangeRequests(*bytes_accept_ranges));

  // Response with an Accept-Ranges header value of |none|. Should return false.
  auto none_accept_ranges = CreateResponse(
      "HTTP/1.1 200 OK\n"
      "Accept-Ranges: none");
  EXPECT_FALSE(
      CorbResponseAnalyzer::SupportsRangeRequests(*none_accept_ranges));
}

}  // namespace network::corb
