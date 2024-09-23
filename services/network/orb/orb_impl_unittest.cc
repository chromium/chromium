// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the ResponseAnalyzerTests (which test the response
// analyzer's behavior in several parameterized test scenarios) and at the end
// includes the CrossOriginReadBlockingTests, which are more typical unittests.

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/orb/orb_impl.h"
#include "services/network/orb/orb_mimetypes.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network::orb {

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

  // Expected result.
  Verdict verdict;
  // The packet number during which the verdict is decided.
  // kVerdictPacketForHeadersBasedVerdict means that the verdict can be decided
  // before the first packet's data is available. |packets.size()| means that
  // the verdict is decided during the end-of-stream call.
  int verdict_packet;
};

inline std::ostream& operator<<(std::ostream& out, const MimeType& value) {
  out << static_cast<int>(value);
  return out;
}

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


  return os << "\n  description           = " << scenario.description
            << "\n  source_line           = " << scenario.source_line
            << "\n  target_url            = " << scenario.target_url
            << "\n  initiator_origin      = " << scenario.initiator_origin
            << "\n  response_headers      = " << response_headers_formatted
            << "\n  response_content_type = " << scenario.response_content_type
            << "\n  canonical_mime_type   = " << scenario.canonical_mime_type
            << "\n  packets               = " << packets
            << "\n  resource_is_sensitive = "
            << (scenario.resource_is_sensitive ? "true" : "false")
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {")]}',\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
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
        {")]}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
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
        {"var x=3;"},                      // packets
        false,                             // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"},  // packets
        false,                                  // resource_is_sensitive
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
        {"{\"x\" : 3}"},                        // packets
        false,                                  // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // first_chunk
        true,                                       // resource_is_sensitive
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
        {")]}'\n[true, false]"},                // packets
        false,                                  // resource_is_sensitive
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
        {"{ \"key\"", ": true }"},              // packets
        false,                                  // resource_is_sensitive
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
        {},                                // packets
        false,                             // resource_is_sensitive
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
        {},                                 // packets
        false,                              // resource_is_sensitive
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
        {"foo({\"x\" : 3})"},              // packets
        false,                             // resource_is_sensitive
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
        {"var x = 3;"},                    // packets
        false,                             // resource_is_sensitive
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
        {"{", "    \n", "var x = 3;\n", "console.log('hello');"},  // packets
        false,  // resource_is_sensitive
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
        {"invoke({ \"key\": true });"},    // packets
        false,                             // resource_is_sensitive
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
        {"[1, 2, {}, true, false, \"yay\"]"},  // packets
        false,                                 // resource_is_sensitive
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
        {"[1, 2, {}, true, false, \"yay\"]", ".map(x => console.log(x))",
         ".map(x => console.log(x));"},  // packets
        false,                           // resource_is_sensitive
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
        {"Won't sniff as XML"},            // packets
        false,                             // resource_is_sensitive
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
        {"Won't sniff as JSON"},           // packets
        false,                             // resource_is_sensitive
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
        {"<htm"},                          // packets
        false,                             // resource_is_sensitive
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
        {kHTMLWithTooLongComment},         // packets
        false,                             // resource_is_sensitive
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
        {},                                // packets
        false,                             // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"Wouldn't sniff as HTML"},             // packets
        false,                                  // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,                                  // resource_is_sensitive
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
        {},                                     // packets
        false,                                  // resource_is_sensitive
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
        {"<hTmL><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<hTmL><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {")]}'\n[true, false]"},            // packets
        true,                               // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"},  // packets
        false,  // resource_is_sensitive
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
        {"{\"x\" : 3}"},                   // packets
        false,                             // resource_is_sensitive
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
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        false,                                        // resource_is_sensitive
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
        {"    ", "\t", "<", "?", "x", "m", "l", ">"},  // packets
        false,                                         // resource_is_sensitive
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
        {"    <!--", "\t -", "-", "->", "\n", "<", "s", "c", "r", "i", "p",
         "t"},  // packets
        false,  // resource_is_sensitive
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
        {"    <!--", " <?xml ", "-->\n", "<", "h", "e", "a", "d"},  // packets
        false,  // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<!doc", "type html><html itemscope=\"\" ",
         "itemtype=\"http://schema.org/SearchResultsPage\" ",
         "lang=\"en\"><head>"},  // packets
        false,                   // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // first_chunk
        false,                                      // resource_is_sensitive
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
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
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
        {")]", "}'\n[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
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
        {"for(;;)", ";[true, true, false, \"user@chromium.org\"]"},  // packets
        false,  // resource_is_sensitive
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
        {R"()]}'
            {}
            #header { color: red; } )"},        // packets
        false,                                  // resource_is_sensitive
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
        {R"(  <html>{}\n"
              .id3 {
                background-color: yellow;
              }
              </html> )"},                      // packets
        false,                                  // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"x = 1;"},                             // packets
        false,                                  // resource_is_sensitive
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
        {"simulated *middle*-of-html content"},  // packets
        true,                                    // resource_is_sensitive
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
        {"movie content"},                      // packets
        false,                                  // resource_is_sensitive
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
        {"these middle bytes are unsniffable"},  // packets
        false,                                   // resource_is_sensitive
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
        {"these middle bytes are unsniffable"},  // packets
        false,                                   // resource_is_sensitive
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
        // Body of test response is based on:
        // 1) net/base/mime_sniffer.cc
        // 2) https://mimesniff.spec.whatwg.org/#signature-for-mp4
        {"....ftypmp4...."},                    // packets
        false,                                  // resource_is_sensitive
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
        // Body of test response is based on:
        // 1) net/base/mime_sniffer.cc
        // 2) https://mimesniff.spec.whatwg.org/#signature-for-mp4
        {"MIME type means this doesn't have to sniff as video"},  // packets
        false,                                  // resource_is_sensitive
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
        {"these middle bytes are unsniffable"},  // packets
        false,                                   // resource_is_sensitive
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
        {/* empty body doesn't sniff as html */},  // packets
        false,                                     // resource_is_sensitive
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
        {/* empty body doesn't sniff as html */},  // packets
        false,                                     // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        false,                                  // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
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
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        true,                                         // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
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
        {"    ", "\t", "{", "\"x\" ", "  ", ": 3}"},  // packets
        true,                                         // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        false,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"var x=3;"},                           // packets
        true,                                   // resource_is_sensitive
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
        {/* empty body doesn't sniff as html */},  // packets
        true,                                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"var x=3;"},              // packets
        true,                      // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {"<html><head>this should sniff as HTML"},  // packets
        true,                                       // resource_is_sensitive
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
        {/* empty body doesn't sniff as html */},  // packets
        true,                                      // resource_is_sensitive
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

TEST_P(ResponseAnalyzerTest, OpaqueResponseBlocking) {
  TestScenario scenario = GetParam();
  SCOPED_TRACE(testing::Message()
               << "\nScenario at " << __FILE__ << ":" << scenario.source_line);

  // Unlike CORB, ORB blocks all 206 responses, unless there was an earlier
  // request to the same URL and that earlier request was classified (based on
  // the MIME type or sniffing) as an audio-or-video response.
  std::string_view description = scenario.description;
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
      std::make_unique<OpaqueResponseBlockingAnalyzer>(&per_factory_state);

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

mojom::URLResponseHeadPtr CreateResponse(std::string raw_headers) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(raw_headers));

  auto response = mojom::URLResponseHead::New();
  response->headers = headers;
  return response;
}

TEST(CrossOriginReadBlockingTest, OrbReportsIssuesOnARAResponse) {
  struct {
    std::string header;
    bool expect_report;
  } kTestCases[] = {
      {/*header=*/"", /*expect_report=*/false},
      {"Attribution-Reporting-Register-Source: {}", /*expect_report=*/true},
      {"Attribution-Reporting-Register-Trigger: {}", /*expect_report=*/true},
      {"Attribution-Reporting-Register-OS-Source: {}", /*expect_report=*/true},
      {"Attribution-Reporting-Register-OS-Trigger: {}",
       /*expect_report=*/true},
  };

  for (const auto& test_case : kTestCases) {
    PerFactoryState per_factory_state;
    auto analyzer =
        std::make_unique<OpaqueResponseBlockingAnalyzer>(&per_factory_state);
    auto ara_response = CreateResponse("HTTP/1.1 200 OK\n" + test_case.header);

    // Mark the response as empty s.t. it would normally not report.
    ara_response->content_length = 0;

    analyzer->Init(GURL("https://b.test"),
                   url::Origin::Create(GURL("https://a.test")),
                   mojom::RequestMode::kNoCors,
                   mojom::RequestDestination::kEmpty, *ara_response);
    EXPECT_EQ(analyzer->ShouldReportBlockedResponse(), test_case.expect_report);
  }
}

}  // namespace network::orb
